import init, {
  solve_two_phase,
  solve_big_m,
  solve_two_phase_rational,
  solve_big_m_rational,
} from "./pkg/simplex.js";

await init();

const inputEl = document.getElementById("input");
const inputCountEl = document.getElementById("input-count");
const errorEl = document.getElementById("error");
const resultsEl = document.getElementById("results");
const cbTwoPhase = document.getElementById("cb-twophase");
const cbBigM = document.getElementById("cb-bigm");
const modeEls = document.querySelectorAll('input[name="mode"]');

const getMode = () => [...modeEls].find((r) => r.checked).value; // 'float' | 'rational'

document.getElementById("solve").addEventListener("click", run);
inputEl.addEventListener("input", () => {
  inputCountEl.textContent = `${inputEl.value.length}/512 characters`;
});

function run() {
  errorEl.textContent = "";
  resultsEl.innerHTML = "";
  const input = inputEl.value.trim();
  if (!input) {
    errorEl.textContent = "Please enter a problem.";
    return;
  }

  const rational = getMode() === "rational";
  const methods = [];
  if (cbTwoPhase.checked)
    methods.push({
      label: "Two-Phase",
      fn: rational ? solve_two_phase_rational : solve_two_phase,
    });
  if (cbBigM.checked)
    methods.push({
      label: "Big-M",
      fn: rational ? solve_big_m_rational : solve_big_m,
    });
  if (!methods.length) {
    errorEl.textContent = "Select at least one method.";
    return;
  }

  for (const m of methods) {
    let data;
    try {
      data = JSON.parse(m.fn(input));
      if (data.error) {
        errorEl.textContent = data.error;
        return;
      }
      resultsEl.appendChild(renderMethod(m.label, data));
    } catch (e) {
      errorEl.textContent = e.message;
      return;
    }
  }
}

function renderMethod(label, data) {
  const div = document.createElement("div");
  div.className = "method-result";

  const h2 = document.createElement("h2");
  h2.textContent = label;
  div.appendChild(h2);

  const summary = document.createElement("div");
  summary.className = "summary";
  if (data.result === "Optimal") {
    summary.textContent = `Optimal: ${data.ans}  |  x = [${data.sln.join(", ")}]`;
  } else {
    summary.textContent = data.result;
  }
  div.appendChild(summary);

  let pivotCount = 0;
  data.steps.forEach((step, idx) => {
    const isLast = idx === data.steps.length - 1;
    const det = document.createElement("details");
    det.open = true;
    const sum = document.createElement("summary");
    if (step.pivot === null) {
      sum.innerHTML = isLast
        ? `<span class="step-label">Final tableau</span>`
        : `<span class="step-label">Phase 1 complete — entering Phase 2</span>`;
    } else {
      pivotCount++;
      const [pr, pc] = step.pivot;
      sum.innerHTML = `<span class="step-label">Step ${pivotCount} — pivot row ${pr + 1}, col ${pc + 1}</span>`;
    }
    det.appendChild(sum);
    det.appendChild(renderTable(step, isLast));
    div.appendChild(det);
  });

  return div;
}

function renderTable(step, isLast) {
  const t = step.table;
  const m = t.aug.length;
  const n = t.obj.length;
  const [pivRow, pivCol] = step.pivot ?? [-1, -1];

  const table = document.createElement("table");

  // Header: BV | x1 … xn | RHS
  const hr = table.createTHead().insertRow();
  th(hr, "BV");
  for (let j = 0; j < n; j++) th(hr, `x${j + 1}`);
  th(hr, "RHS");

  const tbody = table.createTBody();

  // Constraint rows
  for (let i = 0; i < m; i++) {
    const row = tbody.insertRow();
    const bvTd = row.insertCell();
    bvTd.textContent = bvName(t.bv[i], n);
    bvTd.className = "bv-col";
    for (let j = 0; j < n; j++) {
      const td = row.insertCell();
      td.textContent = t.cons[i][j];
      if (!isLast && i === pivRow && j === pivCol) td.className = "pivot";
    }
    row.insertCell().textContent = t.aug[i];
  }

  // Objective row (un-negate obj coefficients for display)
  const objRow = tbody.insertRow();
  const objLabel = objRow.insertCell();
  objLabel.textContent = "obj";
  objLabel.className = "obj-row bv-col";
  for (let j = 0; j < n; j++) {
    const td = objRow.insertCell();
    td.textContent = negFmt(t.obj[j]);
    td.className = "obj-row";
  }
  const rhsTd = objRow.insertCell();
  rhsTd.textContent = t.obj_rhs;
  rhsTd.className = "obj-row";

  return table;
}

function th(row, text) {
  const cell = document.createElement("th");
  cell.textContent = text;
  row.appendChild(cell);
}

function bvName(idx, n) {
  return idx < n ? `x${idx + 1}` : `s${idx - n + 1}`;
}

/** Negate a value that may be a fraction string like "-3/5" or a number string. */
function negFmt(v) {
  if (v === "0") return "0";
  if (v.startsWith("-")) return v.slice(1);
  return "-" + v;
}
