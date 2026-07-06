/// Tiny DOM helper for building screens without JSX.

export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  opts: {
    cls?: string;
    text?: string;
    html?: string;
    attrs?: Record<string, string>;
    on?: Record<string, (e: Event) => void>;
  } = {},
  children: (Node | string)[] = [],
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (opts.cls) node.className = opts.cls;
  if (opts.text !== undefined) node.textContent = opts.text;
  if (opts.html) node.innerHTML = opts.html;
  if (opts.attrs) for (const [k, v] of Object.entries(opts.attrs)) node.setAttribute(k, v);
  if (opts.on) for (const [evt, fn] of Object.entries(opts.on)) node.addEventListener(evt, fn);
  node.append(...children);
  return node;
}
