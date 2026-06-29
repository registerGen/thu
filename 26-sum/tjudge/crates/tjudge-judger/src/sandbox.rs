use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    process::{Command, Stdio},
    sync::OnceLock,
    sync::atomic::{AtomicU32, Ordering},
    time::{Duration, Instant},
};

/// Configuration for a single run inside the sandbox.
#[derive(Debug, Clone)]
pub struct SandboxRunConfig {
    /// Command and arguments to execute.
    pub command: Vec<String>,
    /// Working directory inside the sandbox (host path that is bind-mounted).
    pub work_dir: PathBuf,
    /// Files/directories to make available inside the sandbox (host → guest path pairs).
    pub bind_mounts: Vec<(PathBuf, PathBuf)>,
    /// Environment variables to set.
    pub env: HashMap<String, String>,
    /// Wall-clock time limit in milliseconds.
    pub time_limit_ms: u64,
    /// Memory limit in KiB.
    pub memory_limit_kb: u64,
    /// Maximum number of processes (for interactive/communication problems).
    pub max_processes: u32,
    /// Path for stdin. Always set this to a real file — `None` is passed through
    /// to isolate as no `--stdin`, which inherits the controlling terminal's
    /// stdin and can hang a solution that reads stdin.
    pub stdin: Option<PathBuf>,
    /// Path for stdout capture. Always set this — `None` inherits the terminal.
    pub stdout: Option<PathBuf>,
    /// Path for stderr capture.
    pub stderr: Option<PathBuf>,
}

/// The high-level status of a sandbox run.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SandboxStatus {
    Ok,
    TimeLimitExceeded,
    MemoryLimitExceeded,
    RuntimeError { exit_code: Option<i32> },
    InternalError(String),
}

/// Results returned after a sandbox run completes.
#[derive(Debug, Clone)]
pub struct SandboxResult {
    pub status: SandboxStatus,
    pub wall_time_ms: u64,
    pub cpu_time_ms: u64,
    pub memory_kb: u64,
}

/// Abstraction over a sandbox implementation; mockable in tests.
pub trait Sandbox: Send + Sync {
    /// Run a single command inside the sandbox and return the result.
    fn run(&self, config: &SandboxRunConfig) -> SandboxResult;
    /// The directory on the host that serves as the sandbox root (the judger
    /// uses `box_dir().join("box")` as the working directory it copies files
    /// into).
    fn box_dir(&self) -> &Path;
}

// ---------------------------------------------------------------------------
// JudgerSandbox — isolate with a direct (unsandboxed) fallback
// ---------------------------------------------------------------------------

/// A sandbox that uses [`IsolateSandbox`] when the `isolate` binary is
/// available, and falls back to [`DirectSandbox`] (no isolation) otherwise.
///
/// The fallback is "best effort": it enforces the wall-clock time limit and
/// reports CPU time / peak memory via `getrusage`, but it does **not** enforce
/// the memory limit, the process-count limit, or filesystem/network isolation.
/// It exists so judging still works on machines without `isolate` (e.g. CI,
/// containers without cgroup permissions) — not as a secure substitute.
pub enum JudgerSandbox {
    Isolate(IsolateSandbox),
    Direct(DirectSandbox),
}

static ISOLATE_AVAILABLE: OnceLock<bool> = OnceLock::new();

impl JudgerSandbox {
    /// Try to create an isolate-backed sandbox; fall back to a direct sandbox
    /// if `isolate` is missing or fails to initialize.
    pub fn new() -> Result<Self, String> {
        let have_isolate = *ISOLATE_AVAILABLE.get_or_init(isolate_available);
        // If isolate is available and initializes, use it; otherwise fall back
        // to the direct (unsandboxed) implementation.
        if have_isolate && let Ok(s) = IsolateSandbox::new() {
            return Ok(JudgerSandbox::Isolate(s));
        }
        DirectSandbox::new().map(JudgerSandbox::Direct)
    }
}

impl Sandbox for JudgerSandbox {
    fn run(&self, cfg: &SandboxRunConfig) -> SandboxResult {
        match self {
            JudgerSandbox::Isolate(s) => s.run(cfg),
            JudgerSandbox::Direct(s) => s.run(cfg),
        }
    }
    fn box_dir(&self) -> &Path {
        match self {
            JudgerSandbox::Isolate(s) => s.box_dir(),
            JudgerSandbox::Direct(s) => s.box_dir(),
        }
    }
}

/// Probe once whether the `isolate` binary is installed and runnable.
fn isolate_available() -> bool {
    Command::new("isolate")
        .arg("--version")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

// ---------------------------------------------------------------------------
// IsolateSandbox — wraps the `isolate` binary
// ---------------------------------------------------------------------------

static NEXT_BOX_ID: AtomicU32 = AtomicU32::new(0);

/// A sandbox backed by the `isolate` tool (https://github.com/ioi/isolate).
///
/// Each `IsolateSandbox` owns a unique box ID.  The box is initialised on
/// construction and cleaned up when the value is dropped.
pub struct IsolateSandbox {
    box_id: u32,
    box_dir: PathBuf,
}

impl IsolateSandbox {
    /// Allocate and initialise a new isolate box.
    ///
    /// Box ids are process-unique (a monotonic counter), but isolate boxes live
    /// in a system-global namespace keyed by id. A previous run that crashed
    /// (e.g. a panic) can leave a stale box behind, making `--init` fail with
    /// "box already initialized". On such a failure we clean the stale box up
    /// and retry, then fall back to the next id — so transient races on box ids
    /// no longer abort judging.
    pub fn new() -> Result<Self, String> {
        let mut last_err = String::new();
        for _ in 0..16 {
            let box_id = NEXT_BOX_ID.fetch_add(1, Ordering::Relaxed);
            match Self::init_box(box_id) {
                Ok(box_dir) => return Ok(IsolateSandbox { box_id, box_dir }),
                Err(e) => {
                    last_err = e;
                    // A stale box from a prior crashed run may be in the way.
                    Self::cleanup_box(box_id);
                    if let Ok(box_dir) = Self::init_box(box_id) {
                        return Ok(IsolateSandbox { box_id, box_dir });
                    }
                    // Otherwise try the next id.
                }
            }
        }
        Err(format!("isolate --init failed after retries: {last_err}"))
    }

    fn init_box(box_id: u32) -> Result<PathBuf, String> {
        let output = Command::new("isolate")
            .args(["--box-id", &box_id.to_string(), "--init"])
            .output()
            .map_err(|e| format!("failed to run isolate --init: {e}"))?;
        if !output.status.success() {
            return Err(format!(
                "isolate --init failed: {}",
                String::from_utf8_lossy(&output.stderr)
            ));
        }
        // isolate prints the box directory on stdout
        Ok(PathBuf::from(
            String::from_utf8_lossy(&output.stdout).trim().to_string(),
        ))
    }

    fn cleanup_box(box_id: u32) {
        // Suppress stdout/stderr: isolate prints "This box is currently used
        // by another process" (etc.) on failure, which would leak onto the TUI.
        let _ = Command::new("isolate")
            .args(["--box-id", &box_id.to_string(), "--cleanup"])
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status();
    }

    /// Box directory on the host (contains a `box/` subdirectory).
    pub fn box_dir(&self) -> &Path {
        &self.box_dir
    }
}

impl Drop for IsolateSandbox {
    fn drop(&mut self) {
        Self::cleanup_box(self.box_id);
    }
}

impl Sandbox for IsolateSandbox {
    fn run(&self, cfg: &SandboxRunConfig) -> SandboxResult {
        let mut args: Vec<String> = Vec::new();

        args.push("--box-id".into());
        args.push(self.box_id.to_string());

        args.push(format!("--time={:.3}", cfg.time_limit_ms as f64 / 1000.0));
        // Wall-clock safety net: a run that hangs without consuming CPU (e.g.
        // blocked on inherited stdin/terminal I/O) is never killed by `--time`.
        // Use a generous multiple of the CPU limit so I/O-bound but correct
        // solutions are unaffected, while true hangs are terminated as TLE.
        let wall_s = (cfg.time_limit_ms + 2000) as f64 / 1000.0;
        args.push(format!("--wall-time={:.3}", wall_s));
        args.push(format!("--mem={}", cfg.memory_limit_kb));
        args.push(format!("--processes={}", cfg.max_processes));
        // Share the parent's network namespace instead of creating a per-box
        // one. isolate's default brings up `lo` in a new net namespace, which
        // fails (ENOSYS on SIOCSIFFLAGS) on WSL2 — breaking every run.
        // Network isolation is sacrificed, but filesystem/process/memory/CPU
        // isolation via cgroups is unaffected.
        // args.push("--share-net".into());

        // Bind mounts
        for (host, guest) in &cfg.bind_mounts {
            args.push(format!("--dir={}={}:rw", guest.display(), host.display()));
        }

        // Environment variables
        for (k, v) in &cfg.env {
            args.push(format!("--env={k}={v}"));
        }

        if let Some(p) = &cfg.stdin {
            args.push(format!("--stdin={}", p.display()));
        }
        if let Some(p) = &cfg.stdout {
            args.push(format!("--stdout={}", p.display()));
        }
        if let Some(p) = &cfg.stderr {
            args.push(format!("--stderr={}", p.display()));
        }

        // Write meta to a temp file
        let meta_file = tempfile::NamedTempFile::new().expect("tempfile");
        args.push(format!("--meta={}", meta_file.path().display()));

        args.push("--run".into());
        args.push("--".into());
        for part in &cfg.command {
            args.push(part.clone());
        }

        let status = Command::new("isolate")
            .args(&args)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status();

        // Parse the meta file regardless of exit code
        let meta = parse_meta(meta_file.path());

        let wall_time_ms = meta.get("time-wall").and_then(|s| parse_ms(s)).unwrap_or(0);
        let cpu_time_ms = meta.get("time").and_then(|s| parse_ms(s)).unwrap_or(0);
        let memory_kb = meta
            .get("max-rss")
            .and_then(|s| s.parse().ok())
            .unwrap_or(0);

        let sandbox_status = if let Err(e) = status {
            SandboxStatus::InternalError(format!("isolate exec failed: {e}"))
        } else {
            let killed = meta.get("killed").map(|s| s == "1").unwrap_or(false);
            let exitsig = meta.get("exitsig").and_then(|s| s.parse::<i32>().ok());
            let exitcode = meta.get("exitcode").and_then(|s| s.parse::<i32>().ok());
            let status_str = meta.get("status").map(|s| s.as_str()).unwrap_or("");

            match status_str {
                "TO" => SandboxStatus::TimeLimitExceeded,
                "SG" if killed => {
                    // SIGKILL from memory limit typically shows up as RE with exitsig
                    if meta
                        .get("message")
                        .map(|m| m.contains("Memory limit"))
                        .unwrap_or(false)
                    {
                        SandboxStatus::MemoryLimitExceeded
                    } else {
                        SandboxStatus::RuntimeError {
                            exit_code: exitsig.map(|s| -s),
                        }
                    }
                }
                "RE" | "SG" => SandboxStatus::RuntimeError {
                    exit_code: exitcode,
                },
                "XX" => SandboxStatus::InternalError(
                    meta.get("message")
                        .cloned()
                        .unwrap_or_else(|| "isolate internal error".into()),
                ),
                _ => SandboxStatus::Ok,
            }
        };

        SandboxResult {
            status: sandbox_status,
            wall_time_ms,
            cpu_time_ms,
            memory_kb,
        }
    }

    fn box_dir(&self) -> &Path {
        &self.box_dir
    }
}

// ---------------------------------------------------------------------------
// DirectSandbox — no-isolation fallback
// ---------------------------------------------------------------------------

/// A non-isolating sandbox: runs the command directly on the host, inside a
/// private temporary working directory.
///
/// Used when `isolate` is unavailable. Enforces the wall-clock time limit (by
/// `SIGKILL`ing the whole process group) and reports CPU time and peak RSS via
/// `wait4`/`getrusage`. Does **not** enforce memory, process-count, filesystem,
/// or network isolation — see [`JudgerSandbox`].
pub struct DirectSandbox {
    /// A private temp directory; `box/` inside it is the working directory the
    /// judger copies files into (mirroring isolate's `box_dir/box`).
    box_dir: PathBuf,
}

impl DirectSandbox {
    pub fn new() -> Result<Self, String> {
        let dir =
            tempfile::tempdir().map_err(|e| format!("failed to create direct sandbox dir: {e}"))?;
        let box_dir = dir.keep();
        // Pre-create the `box` subdir so the judger can copy into it directly.
        std::fs::create_dir_all(box_dir.join("box"))
            .map_err(|e| format!("failed to create box subdir: {e}"))?;
        Ok(DirectSandbox { box_dir })
    }

    pub fn box_dir(&self) -> &Path {
        &self.box_dir
    }
}

impl Drop for DirectSandbox {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.box_dir);
    }
}

impl Sandbox for DirectSandbox {
    fn run(&self, cfg: &SandboxRunConfig) -> SandboxResult {
        use std::os::unix::process::CommandExt;

        if cfg.command.is_empty() {
            return SandboxResult {
                status: SandboxStatus::InternalError("empty command".into()),
                wall_time_ms: 0,
                cpu_time_ms: 0,
                memory_kb: 0,
            };
        }

        let mut cmd = Command::new(&cfg.command[0]);
        cmd.args(&cfg.command[1..]);
        cmd.current_dir(&cfg.work_dir);
        cmd.env_clear();
        for (k, v) in &cfg.env {
            cmd.env(k, v);
        }
        cmd.stdin(open_for_read(cfg.stdin.as_deref()));
        cmd.stdout(open_for_write(cfg.stdout.as_deref()));
        cmd.stderr(open_for_write(cfg.stderr.as_deref()));
        // Put the child in its own process group so a TLE can kill the whole
        // tree (the solution may have forked children).
        cmd.process_group(0);

        let start = Instant::now();
        let mut child = match cmd.spawn() {
            Ok(c) => c,
            Err(e) => {
                return SandboxResult {
                    status: SandboxStatus::InternalError(format!("spawn failed: {e}")),
                    wall_time_ms: 0,
                    cpu_time_ms: 0,
                    memory_kb: 0,
                };
            }
        };
        let pid = child.id() as libc::pid_t;

        // Wall-clock ceiling matching isolate's backstop: CPU limit + 2 s. A
        // run still alive after this is killed as TLE.
        let wall_limit_ms = cfg.time_limit_ms.saturating_add(2000);

        let mut cpu_time_ms = 0u64;
        let mut memory_kb = 0u64;
        let mut status_raw: libc::c_int = 0;
        let mut timed_out = false;
        let mut reaped = false;

        while !reaped {
            let mut rusage: libc::rusage = unsafe { std::mem::zeroed() };
            let ret = unsafe { libc::wait4(pid, &mut status_raw, libc::WNOHANG, &mut rusage) };
            if ret == pid {
                cpu_time_ms = rusage_to_ms(&rusage);
                // On Linux, ru_maxrss is in KiB.
                memory_kb = rusage.ru_maxrss.max(0) as u64;
                reaped = true;
            } else if ret < 0 {
                // EINTR is common; ECHILD means it's already gone.
                let errno = unsafe { *libc::__errno_location() };
                if errno == libc::ECHILD {
                    reaped = true;
                } else {
                    std::thread::sleep(Duration::from_millis(2));
                }
            } else {
                // ret == 0: still running. Check the wall-clock limit.
                if !timed_out && start.elapsed().as_millis() as u64 > wall_limit_ms {
                    timed_out = true;
                    // Kill the whole process group (negative pid).
                    unsafe { libc::kill(-pid, libc::SIGKILL) };
                }
                std::thread::sleep(Duration::from_millis(5));
            }
        }
        // Ensure the std::process::Child handle doesn't try to wait itself.
        let _ = child.try_wait();

        let wall_time_ms = start.elapsed().as_millis() as u64;

        let status = if timed_out {
            SandboxStatus::TimeLimitExceeded
        } else if libc::WIFEXITED(status_raw) {
            let code = libc::WEXITSTATUS(status_raw);
            if code == 0 {
                SandboxStatus::Ok
            } else {
                SandboxStatus::RuntimeError {
                    exit_code: Some(code as i32),
                }
            }
        } else if libc::WIFSIGNALED(status_raw) {
            let sig = libc::WTERMSIG(status_raw);
            SandboxStatus::RuntimeError {
                exit_code: Some(-sig),
            }
        } else {
            SandboxStatus::RuntimeError { exit_code: None }
        };

        SandboxResult {
            status,
            wall_time_ms,
            cpu_time_ms,
            memory_kb,
        }
    }

    fn box_dir(&self) -> &Path {
        &self.box_dir
    }
}

/// Open a file for the sandbox's stdin, or `/dev/null` if unset.
fn open_for_read(path: Option<&Path>) -> Stdio {
    match path {
        Some(p) => std::fs::File::open(p)
            .map(Stdio::from)
            .unwrap_or(Stdio::null()),
        None => Stdio::null(),
    }
}

/// Open (or create) a file for the sandbox's stdout/stderr, or `/dev/null`.
fn open_for_write(path: Option<&Path>) -> Stdio {
    match path {
        Some(p) => std::fs::OpenOptions::new()
            .create(true)
            .write(true)
            .truncate(true)
            .open(p)
            .map(Stdio::from)
            .unwrap_or(Stdio::null()),
        None => Stdio::null(),
    }
}

/// Convert a `getrusage` struct to total CPU time in milliseconds.
fn rusage_to_ms(r: &libc::rusage) -> u64 {
    let user = r.ru_utime.tv_sec as u64 * 1000 + r.ru_utime.tv_usec as u64 / 1000;
    let sys = r.ru_stime.tv_sec as u64 * 1000 + r.ru_stime.tv_usec as u64 / 1000;
    user + sys
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Parse an isolate meta file into a key→value map.
fn parse_meta(path: &Path) -> HashMap<String, String> {
    let Ok(content) = std::fs::read_to_string(path) else {
        return HashMap::new();
    };
    content
        .lines()
        .filter_map(|l| {
            let (k, v) = l.split_once(':')?;
            Some((k.to_string(), v.to_string()))
        })
        .collect()
}

/// Parse a seconds string (e.g. "1.234") into milliseconds.
fn parse_ms(s: &str) -> Option<u64> {
    let secs: f64 = s.parse().ok()?;
    Some((secs * 1000.0) as u64)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    fn cfg(
        work_dir: &Path,
        command: Vec<String>,
        stdout: Option<PathBuf>,
        time_limit_ms: u64,
    ) -> SandboxRunConfig {
        SandboxRunConfig {
            command,
            work_dir: work_dir.to_path_buf(),
            bind_mounts: vec![],
            env: HashMap::new(),
            time_limit_ms,
            memory_limit_kb: 1 << 20,
            max_processes: 1,
            stdin: None,
            stdout,
            stderr: None,
        }
    }

    #[test]
    fn direct_ok_and_nonzero_exit() {
        let sb = DirectSandbox::new().unwrap();
        let work = sb.box_dir().join("box");
        let r = sb.run(&cfg(&work, vec!["true".into()], None, 2000));
        assert_eq!(r.status, SandboxStatus::Ok);
        let r = sb.run(&cfg(&work, vec!["false".into()], None, 2000));
        assert!(
            matches!(r.status, SandboxStatus::RuntimeError { exit_code: Some(1) }),
            "got {:?}",
            r.status
        );
    }

    #[test]
    fn direct_stdout_capture() {
        let sb = DirectSandbox::new().unwrap();
        let work = sb.box_dir().join("box");
        let out = work.join("o.txt");
        let r = sb.run(&cfg(
            &work,
            vec!["sh".into(), "-c".into(), "printf hello".into()],
            Some(out.clone()),
            2000,
        ));
        assert_eq!(r.status, SandboxStatus::Ok);
        assert_eq!(std::fs::read_to_string(&out).unwrap(), "hello");
    }

    #[test]
    fn direct_tle_kills_process_group() {
        let sb = DirectSandbox::new().unwrap();
        let work = sb.box_dir().join("box");
        // time_limit_ms = 0 → wall ceiling = 2s; `sleep 10` must be killed as TLE.
        let r = sb.run(&cfg(
            &work,
            vec!["sh".into(), "-c".into(), "sleep 10".into()],
            None,
            0,
        ));
        assert_eq!(r.status, SandboxStatus::TimeLimitExceeded);
        assert!(r.wall_time_ms < 5000, "wall {}ms", r.wall_time_ms);
    }

    #[test]
    fn direct_reports_cpu_time_and_memory() {
        let sb = DirectSandbox::new().unwrap();
        let work = sb.box_dir().join("box");
        // A little CPU work so ru_utime is non-zero.
        let r = sb.run(&cfg(
            &work,
            vec![
                "sh".into(),
                "-c".into(),
                "i=0; while [ $i -lt 200000 ]; do i=$((i+1)); done".into(),
            ],
            None,
            2000,
        ));
        assert_eq!(r.status, SandboxStatus::Ok);
        assert!(r.cpu_time_ms > 0, "cpu_time {}ms", r.cpu_time_ms);
    }
}
