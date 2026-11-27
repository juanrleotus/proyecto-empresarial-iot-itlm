/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   Node-RED  -  Funcion "Detectar alarmas"

   ENTRADA : mensajes del nodo  mqtt in  suscrito a  empresa/#
   SALIDA  : msg.query + msg.params para el nodo  postgresql
             (tabla alarmas)

   Criterio: no se guarda una alarma por cada medicion fuera de
   rango -- a 5 segundos por muestra serian 720 filas por hora.
   Se guarda solo el CAMBIO de estado: cuando la variable entra
   en alarma y cuando se normaliza. El estado anterior se
   recuerda en el contexto del flujo.

   Pegar en la pestana "En mensaje" (On Message) de un nodo
   function con UNA sola salida.
   ============================================================ */

// ── Limites de proceso ──────────────────────────────────────
// Ajustar a la realidad de la instalacion. Son los valores que
// definen el "funcionamiento normal" del sistema.
const LIMITES = {
    "empresa/planta/temperatura":  { dispositivo: "Planta",  variable: "temperatura", min: 5,  max: 35 },
    "empresa/planta/humedad":      { dispositivo: "Planta",  variable: "humedad",     min: 20, max: 80 },
    "empresa/oficina/temperatura": { dispositivo: "Oficina", variable: "temperatura", min: 16, max: 30 },
    "empresa/oficina/humedad":     { dispositivo: "Oficina", variable: "humedad",     min: 20, max: 80 },
    "empresa/almacen/temperatura": { dispositivo: "Almacen", variable: "temperatura", min: 2,  max: 25 },
    "empresa/tanque/temperatura":  { dispositivo: "Tanque",  variable: "temperatura", min: 4,  max: 45 }
};

const topico = msg.topic;
const limite = LIMITES[topico];

if (!limite) return null;          // topico sin limites definidos

// ── Lectura del valor ───────────────────────────────────────
let datos = msg.payload;
if (Buffer.isBuffer(datos)) datos = datos.toString();
if (typeof datos === "string") {
    try { datos = JSON.parse(datos); } catch (e) { return null; }
}

const valor = Number(datos && datos.v);
if (!Number.isFinite(valor)) return null;

// ── Estado anterior de esta variable ────────────────────────
// flow.get / flow.set conservan el dato entre mensajes.
const clave     = "alarma_" + topico;
const enAlarmaAntes = flow.get(clave) || false;

let enAlarmaAhora = false;
let mensaje       = "";
let limiteVulnerado = null;

if (valor > limite.max) {
    enAlarmaAhora   = true;
    limiteVulnerado = limite.max;
    mensaje = "ALTA: " + limite.variable + " de " + limite.dispositivo +
              " = " + valor + " (maximo " + limite.max + ")";
} else if (valor < limite.min) {
    enAlarmaAhora   = true;
    limiteVulnerado = limite.min;
    mensaje = "BAJA: " + limite.variable + " de " + limite.dispositivo +
              " = " + valor + " (minimo " + limite.min + ")";
}

// ── Solo se registra el CAMBIO de estado ────────────────────
if (enAlarmaAhora === enAlarmaAntes) {
    node.status({ fill: enAlarmaAhora ? "red" : "green",
                  shape: "ring",
                  text: enAlarmaAhora ? "en alarma" : "normal" });
    return null;
}

flow.set(clave, enAlarmaAhora);

if (!enAlarmaAhora) {
    // Normalizacion: tambien es informacion util.
    mensaje = "NORMALIZADA: " + limite.variable + " de " +
              limite.dispositivo + " = " + valor;
    limiteVulnerado = null;
}

msg.query = `
    INSERT INTO alarmas (dispositivo, variable, valor, limite, mensaje)
    VALUES ($1, $2, $3, $4, $5)
    RETURNING id, fecha, mensaje;
`;

msg.params = [limite.dispositivo, limite.variable, valor, limiteVulnerado, mensaje];

node.status({ fill: enAlarmaAhora ? "red" : "green", shape: "dot", text: mensaje });
node.warn(mensaje);

return msg;
