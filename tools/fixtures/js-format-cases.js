// Writes js-format-cases.json: doubles (as bit patterns) and strings with what JavaScript's JSON.stringify makes of them.
// Usage, from the repo root:  node tools/fixtures/js-format-cases.js
// Doubles → JS Number::toString as used by JSON.stringify. Bits are hex so nothing is lost in transit.
const buf = new DataView(new ArrayBuffer(8));
const bits = x => { buf.setFloat64(0, x); return buf.getBigUint64(0).toString(16).padStart(16, '0'); };
const xs = [0, -0, 1, -1, 0.5, 0.1, 0.2, 0.30000000000000004, 1/3, 2/3, 70/150, 2/14, 1e-6, 1e-7, 1.5e-7, 0.000001234, 0.00001,
  123456789, 1e20, 1e21, 1.5e21, 123456789012345680000, 2**53, 2**53 + 2, 5e-324, 1.7976931348623157e308, 0.6916759451407697,
  0.49909478848107497, 100, 1250, 12.5, -0.001, 1e-5 * 3, 9.999999999999999e-7, 1e15, 1e16, 1e17, 4.35, 0.07];
let seed = 42; const r = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
for (let i = 0; i < 200; i++) xs.push(r());
for (let i = 0; i < 120; i++) xs.push((r() - 0.5) * Math.pow(10, Math.floor(r() * 50) - 25));
for (let i = 0; i < 40; i++) xs.push(Math.log(r() * 9 + 1) / Math.log(200));
const out = xs.map(x => [bits(x), JSON.stringify(x)]);
const strs = ['plain', 'quote " and \\ backslash', 'slash / stays', 'tab\tnew\nline\r\b\f', '\u0000\u0001\u001f\u007f', 'Ünïcode “quotes” ♯ 🐛', '  ', 'é'];
require('fs').writeFileSync(require("path").resolve(__dirname, "../../Packages/AWCore/Tests/AWCoreTests/Fixtures/js-format-cases.json"), JSON.stringify({ numbers: out, strings: strs.map(s => [s, JSON.stringify(s)]) }, null, 1));
