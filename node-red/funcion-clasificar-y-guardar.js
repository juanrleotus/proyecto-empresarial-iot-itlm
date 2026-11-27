/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   Node-RED  -  Funcion "Clasificar y guardar"

   ENTRADA : mensajes del nodo  mqtt in  suscrito a  empresa/#
   SALIDA  : msg.query + msg.params para el nodo  postgresql

   Que hace:
     - descarta los topicos de comando (.../cmd)
     - reconoce los topicos de sensores  -> tabla mediciones
     - reconoce los topicos de actuadores -> tabla eventos
     - reconoce los avisos de la camara   -> tabla eventos
     - valida el dato antes de escribir en la base

   Pegar en la pestana  "En mensaje"  (On Message) de un nodo
   function con UNA sola salida.
   ============================================================ */

// ── 1. Lectura del mensaje ──────────────────────────────────
const topico = msg.topic;
let datos    = msg.payload;

if (datos === null || datos === undefined || datos === "") {
    node.warn("Mensaje MQTT vacio en: " + topico);
    return null;
}

// El nodo mqtt in puede entregar el contenido como Buffer
if (Buffer.isBuffer(datos)) {
    datos = datos.toString();
}

// Si llega como texto, interpretarlo como JSON
if (typeof datos === "string") {
    try {
        datos = JSON.parse(datos);
    } catch (error) {
        // Caso tipico: una placa quedo publicando texto plano
        // ("24.8") en lugar del JSON del proyecto.
        node.warn("No es JSON en " + topico + " -> " + datos +
                  "  (revisar el firmware de esa placa)");
        return null;
    }
}

// ── 2. Los comandos NO se guardan ───────────────────────────
// Node-RED esta suscrito a empresa/# y por lo tanto tambien ve
// lo que la app publica en .../cmd. Eso es una orden, no un
// dato medido: no va a la base.
if (topico.endsWith("/cmd")) {
    node.status({ fill: "grey", shape: "ring", text: "cmd ignorado" });
    return null;
}

// ── 3. Tabla de topicos de SENSORES ─────────────────────────
// Agregar un sensor nuevo = agregar UNA linea aqui.
const SENSORES = {
    "empresa/planta/temperatura":  { dispositivo: "Planta",  variable: "temperatura" },
    "empresa/planta/humedad":      { dispositivo: "Planta",  variable: "humedad"     },
    "empresa/oficina/temperatura": { dispositivo: "Oficina", variable: "temperatura" },
    "empresa/oficina/humedad":     { dispositivo: "Oficina", variable: "humedad"     },
    "empresa/almacen/temperatura": { dispositivo: "Almacen", variable: "temperatura" },
    "empresa/tanque/temperatura":  { dispositivo: "Tanque",  variable: "temperatura" }
};

// ── 4. Tabla de topicos de ACTUADORES ───────────────────────
const ACTUADORES = {
    "empresa/planta/aire/estado":   { dispositivo: "Planta",  actuador: "aire"   },
    "empresa/tanque/rele/estado":   { dispositivo: "Tanque",  actuador: "bomba"  },
    "empresa/oficina/luces/estado": { dispositivo: "Oficina", actuador: "luces"  },
    "empresa/camara/estado":        { dispositivo: "Almacen", actuador: "camara" }
};

// ════════════════════════════════════════════════════════════
//  CASO A: es un SENSOR  ->  tabla mediciones
// ════════════════════════════════════════════════════════════
if (SENSORES[topico]) {

    const info  = SENSORES[topico];
    const valor = Number(datos.v);

    // Validacion: sin esto, un NaN o un texto rompe el INSERT
    if (datos.v === undefined || datos.v === null || datos.v === "" ||
        !Number.isFinite(valor)) {
        node.warn("Medicion invalida en " + topico + ": " + JSON.stringify(datos));
        return null;
    }

    // Rechazo de lecturas absurdas (sensor desconectado).
    // El DS18B20 devuelve -127 cuando no responde.
    if (valor < -50 || valor > 150) {
        node.warn("Valor fuera de escala fisica en " + topico + ": " + valor);
        return null;
    }

    const unidad = datos.u || (info.variable === "humedad" ? "%" : "C");

    msg.query = `
        INSERT INTO mediciones (dispositivo, variable, valor, unidad)
        VALUES ($1, $2, $3, $4)
        RETURNING id, fecha, dispositivo, variable, valor, unidad;
    `;

    msg.params = [info.dispositivo, info.variable, valor, unidad];

    node.status({ fill: "green", shape: "dot",
                  text: info.dispositivo + " " + info.variable + " = " + valor + unidad });
    return msg;
}

// ════════════════════════════════════════════════════════════
//  CASO B: es un ACTUADOR  ->  tabla eventos
// ════════════════════════════════════════════════════════════
if (ACTUADORES[topico]) {

    const info   = ACTUADORES[topico];
    const estado = String(datos.v || "").toUpperCase();

    // El aire acondicionado tiene un tercer estado: ESPERA
    // (proteccion de arranque corto del compresor).
    if (!["ON", "OFF", "ESPERA"].includes(estado)) {
        node.warn("Estado no reconocido en " + topico + ": " + JSON.stringify(datos));
        return null;
    }

    // La camara agrega su IP y sus URL al estado: se guardan
    // como detalle para saber donde estaba conectada.
    let detalle = null;
    if (datos.ip)    detalle = "ip=" + datos.ip;
    if (datos.video) detalle = (detalle ? detalle + " " : "") + "video=" + datos.video;

    msg.query = `
        INSERT INTO eventos (dispositivo, actuador, estado, detalle)
        VALUES ($1, $2, $3, $4)
        RETURNING id, fecha, dispositivo, actuador, estado;
    `;

    msg.params = [info.dispositivo, info.actuador, estado, detalle];

    node.status({ fill: "blue", shape: "dot",
                  text: info.actuador + " -> " + estado });
    return msg;
}

// ════════════════════════════════════════════════════════════
//  CASO C: aviso de la camara  ->  tabla eventos
//  Ejemplo de contenido:
//    {"v":"FOTO","u":"evt","ts":123,"url":"http://192.168.1.50/capture?t=123"}
// ════════════════════════════════════════════════════════════
if (topico === "empresa/camara/evento") {

    const evento = String(datos.v || "EVENTO").toUpperCase();
    const url    = datos.url || null;

    msg.query = `
        INSERT INTO eventos (dispositivo, actuador, estado, detalle)
        VALUES ($1, $2, $3, $4)
        RETURNING id, fecha, dispositivo, actuador, estado, detalle;
    `;

    msg.params = ["Almacen", "camara", evento, url];

    node.status({ fill: "yellow", shape: "dot", text: "camara: " + evento });
    return msg;
}

// ════════════════════════════════════════════════════════════
//  CASO D: topico desconocido
//  No es un error del sistema: puede ser una placa nueva o un
//  topico de prueba. Se avisa y no se guarda nada.
// ════════════════════════════════════════════════════════════
node.status({ fill: "grey", shape: "ring", text: "topico sin uso" });
node.warn("Topico no contemplado: " + topico);
return null;
