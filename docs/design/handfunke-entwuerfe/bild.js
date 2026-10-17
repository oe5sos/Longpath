/* bild.js — Attrappendaten fuer die Entwurfsblaetter.
   Deterministisch (eigener Zufall mit festem Keim), damit zwei Blaetter
   dieselbe Bandbelegung zeigen und man die GESTALTUNG vergleicht, nicht
   zufaellig verschiedene Signale. */
let keim = 20261001;
function zuf(){ keim = (keim * 1103515245 + 12345) & 0x7fffffff; return keim / 0x7fffffff; }

const TRAEGER = [[0.17,0.62],[0.31,0.44],[0.50,0.95],[0.63,0.40],[0.78,0.70]];

function spektrum(n){
  const a = new Float32Array(n);
  for (let i=0;i<n;i++){
    const t = i/n;
    let v = 0.10 + zuf()*0.055;
    for (const [m,h] of TRAEGER){
      const d = (t-m)/0.009;
      v += h * Math.exp(-d*d);
    }
    a[i] = Math.min(1, v);
  }
  return a;
}

/* Panadapter: Kurve mit Fuellung darunter. */
function malPan(c, farbe, fuellung){
  const g = c.getContext('2d'), W = c.width, H = c.height;
  g.clearRect(0,0,W,H);
  const a = spektrum(W);
  if (fuellung){
    const v = g.createLinearGradient(0,0,0,H);
    v.addColorStop(0, fuellung[0]); v.addColorStop(1, fuellung[1]);
    g.beginPath(); g.moveTo(0,H);
    for (let x=0;x<W;x++) g.lineTo(x, H - a[x]*H*0.92);
    g.lineTo(W,H); g.closePath(); g.fillStyle = v; g.fill();
  }
  g.beginPath();
  for (let x=0;x<W;x++){ const y = H - a[x]*H*0.92; x?g.lineTo(x,y):g.moveTo(x,y); }
  g.strokeStyle = farbe; g.lineWidth = 1.4; g.lineJoin='round'; g.stroke();
}

/* Wasserfall: echte Farbskala statt Grau. Dunkelblau -> Bernstein -> Weiss. */
function skala(v){
  if (v < 0.30) { const t=v/0.30;            return [20+t*16, 30+t*26, 44+t*34]; }
  if (v < 0.62) { const t=(v-0.30)/0.32;     return [36+t*134, 56+t*74, 78-t*6]; }
  if (v < 0.86) { const t=(v-0.62)/0.24;     return [170+t*62, 130+t*48, 72+t*40]; }
  const t=(v-0.86)/0.14;                     return [232+t*20, 178+t*60, 112+t*120];
}
function malWf(c){
  const g = c.getContext('2d'), W=c.width, H=c.height;
  const bild = g.createImageData(W,H);
  for (let y=0;y<H;y++){
    const a = spektrum(W);
    const alter = 1 - y/H*0.45;            // nach unten blasser: das ist Zeit
    for (let x=0;x<W;x++){
      const [r,gr,b] = skala(Math.min(1, a[x]*alter*1.25));
      const i = (y*W+x)*4;
      bild.data[i]=r; bild.data[i+1]=gr; bild.data[i+2]=b; bild.data[i+3]=255;
    }
  }
  g.putImageData(bild,0,0);
}
