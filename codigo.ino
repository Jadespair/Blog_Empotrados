#include <LiquidCrystal.h>
#include <Thread.h>
#include <DHT.h>
#include <avr/wdt.h>

// Watchdog
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init(void) { MCUSR = 0; wdt_disable(); }  // Limpia MCUSR y desactiva WDT antes de setup()

// Constantes
const int PIN_LED1 = 4;
const int PIN_LED2 = 9;
const int PIN_TRIGGER = 7;
const int PIN_ECHO = 8;
const int PIN_DHT = 6;
const int PIN_JOY_X = A1;
const int PIN_JOY_Y = A2;
const int PIN_JOY_SW = 3;
const int PIN_BOTON_EXT = 2;

const float DISTANCIA_DETECCION = 150.0;    // Umbral para detectar cliente
const float DISTANCIA_SALIDA = 180.0;       // Umbral para considerar que se fue (histéresis)

// Objetos globales
LiquidCrystal lcd(A0, 13, A3, 5, 11, 12);
DHT dht(PIN_DHT, DHT11);

// Threads (hilos)
Thread threadSensor, threadLCD, threadDHT, threadJoystick;

// Lista de productos
struct Producto { const char* nombre; float precio; };
Producto catalogo[] = {
  {"Cafe Solo    ", 1.00},
  {"Cafe Cortado ", 1.10},
  {"Cafe Doble   ", 1.25},
  {"Cafe Premium ", 1.50},
  {"Chocolate    ", 2.00}
};
const int TOTAL_PRODUCTOS = 5;
int indiceProducto = 0;

// Opciones del menú Admin
const int OPC_TEMP = 0, OPC_DIST = 1, OPC_TIME = 2, OPC_PRECIOS = 3, NUM_OPCIONES_ADMIN = 4;
const char* const OPCIONES_ADMIN[NUM_OPCIONES_ADMIN] = {
  "1.Ver Temp/Hum", "2.Ver Distancia", "3.Ver Contador", "4.Modif. Precios"
};
int adminOpcion = OPC_TEMP;
bool enVistaAdmin = false;
int indicePrecioAdmin = 0;
float precioTemporal;

// Máquina de estados
enum Estado { ARRANCANDO, ESPERANDO, BIENVENIDA, MENU, PREPARANDO, RETIRAR, EXITO_RETIRADA, ADMIN };
Estado estadoActual = ARRANCANDO;
unsigned long tiempoInicioEstado, tiempoSalidaMenu, duracionPreparacion;
float distanciaActual = 400.0, temperatura = 0.0, humedad = 0.0;
int contadorDeteccion = 0;

// Interrupción de botón externo
volatile bool botonExtPresionado = false;
volatile unsigned long tiempoInicioPulsacion = 0;
bool resetEjecutado = false, adminEjecutado = false;

// Rutina de servicio de la interrupción: guarda el instante de la pulsación
void isrBotonExterno() {
  if (digitalRead(PIN_BOTON_EXT) == LOW) {
    botonExtPresionado = true;
    tiempoInicioPulsacion = millis();
  } else {
    botonExtPresionado = false;
  }
}

// Funciones auxiliares
void configurarHilo(Thread& hilo, int ms, void (*func)()) {     // Configura intervalo, tarea y activa el hilo
  hilo.setInterval(ms);
  hilo.onRun(func);
  hilo.enabled = true;
}

void printLinea(int fila, String texto) {     // Rellena a 16 chars y recorta para limpiar la línea
  lcd.setCursor(0, fila);
  while (texto.length() < 16) texto += " ";
  lcd.print(texto.substring(0, 16));
}

int leerDireccionY() {      // Devuelve -1 arriba, +1 abajo, 0 en reposo
  int y = analogRead(PIN_JOY_Y);
  return (y < 300) ? -1 : (y > 700) ? 1 : 0;
}

bool joystickIzquierda() {      // True si el joystick se mueve a la izquierda
  return analogRead(PIN_JOY_X) < 300;
}

void mostrarTempHum() {     // Pinta temperatura y humedad en el LCD
  printLinea(0, "Temp: " + String(temperatura, 1) + " C");
  printLinea(1, "Hum:  " + String(humedad, 0) + " %");
}

// Hilo del sensor de ultrasonidos
void tareaSensor() {
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);      // Pulso de trigger de 10 µs del HC-SR04
  digitalWrite(PIN_TRIGGER, LOW);
  long duracion = pulseIn(PIN_ECHO, HIGH, 30000);     // Espera el eco con timeout de 30 ms
  float dist = duracion * 0.01715;      // Convierte µs a cm
  distanciaActual = (duracion > 0 && dist >= 2.0 && dist <= 400.0) ? dist : 400.0;      // Filtra ecos inválidos
}

// Hilo del sensor DHT11
void tareaDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { temperatura = t; humedad = h; }   // Solo guarda lecturas válidas
}

// Hilo del joystick
void tareaJoystick() {
  static unsigned long t_mov = 0;       // Marca de tiempo del último movimiento
  static unsigned long t_click = 0;     // Marca de tiempo del último clic
  static bool botonSuelto = true;       // Indica si el botón está en reposo

  int dirY = leerDireccionY();
  bool click = (digitalRead(PIN_JOY_SW) == LOW);

  // Antirrebote: exige flanco (suelto→pulsado) y 200 ms desde el último clic
  bool clickRegistrado = false;
  if (click && botonSuelto && (millis() - t_click > 200)) {
    clickRegistrado = true;
    botonSuelto = false;
    t_click = millis();
  } else if (!click) {
    botonSuelto = true;
  }

  bool listo = (millis() - t_mov > 200);      // Antirrebote del eje Y (200 ms)

  // Navegación en el menú de productos
  if (estadoActual == MENU) {
    if (listo && dirY) {        
      indiceProducto = (indiceProducto + dirY + TOTAL_PRODUCTOS) % TOTAL_PRODUCTOS;
      t_mov = millis();
    }
    if (clickRegistrado) {       // Inicia preparación con tiempo aleatorio 4-8 s
      duracionPreparacion = random(4000, 8001);
      estadoActual = PREPARANDO;
      tiempoInicioEstado = millis();
    }
  }
  // Navegación en el modo Admin
  else if (estadoActual == ADMIN) {
    if (!enVistaAdmin) {          // Navegación por el menú principal de Admin
      if (listo && dirY) {
        adminOpcion = (adminOpcion + dirY + NUM_OPCIONES_ADMIN) % NUM_OPCIONES_ADMIN;
        t_mov = millis();
      }
      if (clickRegistrado) {      // Entra en la vista de la opción seleccionada
        enVistaAdmin = true;
        if (adminOpcion == OPC_PRECIOS) {
          indicePrecioAdmin = 0;
          precioTemporal = catalogo[0].precio;
        }
      }
    } else {
      if (adminOpcion == OPC_PRECIOS) {     // Editor de precios
        if (listo && dirY) {      // Sube/baja 5 céntimos, limitado a [0.05, 99.95]
          precioTemporal = constrain(precioTemporal + dirY * 0.05, 0.05, 99.95);
          t_mov = millis();
        }
        if (clickRegistrado) {      // Confirma y pasa al siguiente producto
          catalogo[indicePrecioAdmin].precio = precioTemporal;
          if (++indicePrecioAdmin >= TOTAL_PRODUCTOS) enVistaAdmin = false;
          else precioTemporal = catalogo[indicePrecioAdmin].precio;
        }
      }
      if (listo && joystickIzquierda()) { enVistaAdmin = false; t_mov = millis(); }     // Izquierda: volver
    }
  }
}

// Hilo de pantalla LCD
void tareaLCD() {   // Dibuja en el LCD según el estado actual 
  switch (estadoActual) {
    case ARRANCANDO:
      printLinea(0, "INICIANDO...");
      break;

    case ESPERANDO:  // Pantalla en reposo esperando cliente
      printLinea(0, "ESPERANDO");
      printLinea(1, "CLIENTE");
      break;

    case BIENVENIDA:  // Temperatura y humedad durante 5 s
      mostrarTempHum();
      break;

    case MENU:  // Producto seleccionado y su precio
      printLinea(0, catalogo[indiceProducto].nombre);
      printLinea(1, String(catalogo[indiceProducto].precio, 2) + " EUR");
      break;

    case PREPARANDO:  // Bebida en preparación
      printLinea(0, "Preparando...");
      printLinea(1, catalogo[indiceProducto].nombre);
      break;

    case RETIRAR:  // Pide retirar la bebida
      printLinea(0, "RETIRE BEBIDA");
      printLinea(1, "");
      break;

    case EXITO_RETIRADA:  // Confirmación de retirada
      printLinea(0, "RETIRADA EXITOSA");
      printLinea(1, "¡Gracias!");
      break;

    case ADMIN:
      if (!enVistaAdmin) {  // Menú principal de Admin
        printLinea(0, "== MODO ADMIN ==");
        printLinea(1, OPCIONES_ADMIN[adminOpcion]);
      } else if (adminOpcion == OPC_TEMP) {
        mostrarTempHum();
      } else if (adminOpcion == OPC_DIST) {
        printLinea(1, String(distanciaActual, 1) + " cm");
      } else if (adminOpcion == OPC_TIME) {  // Contador de segundos desde el encendido
        printLinea(1, String(millis() / 1000) + " s");
      } else if (adminOpcion == OPC_PRECIOS) {  // Producto y precio temporal
        printLinea(0, catalogo[indicePrecioAdmin].nombre);
        printLinea(1, String(precioTemporal, 2) + " EUR");
      }
      break;
  }
}

void setup() {
  dht.begin();
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_BOTON_EXT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BOTON_EXT), isrBotonExterno, CHANGE);  // ISR en flanco

  randomSeed(analogRead(A0));  
  lcd.begin(16, 2);

  estadoActual = ARRANCANDO;    // Estado inicial 
  tiempoInicioEstado = millis();
  digitalWrite(PIN_LED1, HIGH);
  analogWrite(PIN_LED2, 0);

  // Configuración de los hilos
  configurarHilo(threadSensor,   100,  tareaSensor);    // Ultrasónicos cada 100 ms
  configurarHilo(threadLCD,      200,  tareaLCD);       // LCD cada 200 ms
  configurarHilo(threadDHT,      2000, tareaDHT);       // DHT11 cada 2 s
  configurarHilo(threadJoystick, 50,   tareaJoystick);  // Joystick cada 50 ms

  wdt_enable(WDTO_2S);    // Watchdog con timeout de 2 s
}

void loop() {
  wdt_reset();    // Alimenta al Watchdog

  // Ejecución de los hilos según su intervalo
  if (threadSensor.shouldRun())   threadSensor.run();
  if (threadLCD.shouldRun())      threadLCD.run();
  if (threadDHT.shouldRun())      threadDHT.run();
  if (threadJoystick.shouldRun()) threadJoystick.run();

  // Gestión del botón externo (ignorado durante ARRANCANDO)
  if (estadoActual != ARRANCANDO) {
    if (botonExtPresionado) {
      unsigned long t = millis() - tiempoInicioPulsacion;   // Tiempo mantenido

      // ≥5 s: entra o sale del modo Admin
      if (t >= 5000 && !adminEjecutado) {
        adminEjecutado = true;
        bool entrando = (estadoActual != ADMIN);
        estadoActual = entrando ? ADMIN : ESPERANDO;
        enVistaAdmin = false;
        adminOpcion = OPC_TEMP;
        digitalWrite(PIN_LED1, entrando);           // LED1 encendido en Admin
        analogWrite(PIN_LED2, entrando ? 255 : 0);  // LED2 encendido en Admin
        if (!entrando) tiempoSalidaMenu = millis();
      }
      // 2-3 s: reset del servicio
      else if (t >= 2000 && t < 3000 && !resetEjecutado && estadoActual != ADMIN) {
        resetEjecutado = true;
        estadoActual = ESPERANDO;
        analogWrite(PIN_LED2, 0);
        tiempoSalidaMenu = millis();
      }
    } else {  // Al soltar, resetea banderas para permitir nueva pulsación
      adminEjecutado = false;
      resetEjecutado = false;
    }
  }

  // Máquina de estados
  switch (estadoActual) {
    case ARRANCANDO: {
      unsigned long t = millis() - tiempoInicioEstado;
      if (t < 6000) {
        digitalWrite(PIN_LED1, (t / 1000) % 2 == 0);    // Parpadeo 
      } else {
        digitalWrite(PIN_LED1, LOW);
        estadoActual = ESPERANDO;
        tiempoSalidaMenu = millis();
      }
      break;
    }

    case ESPERANDO:   // Exige 3 lecturas <150 cm seguidas para confirmar cliente
      if (distanciaActual < DISTANCIA_DETECCION && (millis() - tiempoSalidaMenu > 2000)) {
        if (++contadorDeteccion >= 3) {
          estadoActual = BIENVENIDA;
          tiempoInicioEstado = millis();
          contadorDeteccion = 0;
        }
      } else contadorDeteccion = 0;
      break;

    case BIENVENIDA:    // Muestra temperatura/humedad 5 s y pasa al menú
      if (millis() - tiempoInicioEstado >= 5000) {
        estadoActual = MENU;
        indiceProducto = 0;
      }
      break;

    case MENU:    // Si el cliente se aleja >180 cm, vuelve a esperar
      if (distanciaActual > DISTANCIA_SALIDA) {
        estadoActual = ESPERANDO;
        tiempoSalidaMenu = millis();
      }
      break;

    case PREPARANDO: {
      unsigned long t = millis() - tiempoInicioEstado;
      if (t >= duracionPreparacion) {  // Fin de preparación
        analogWrite(PIN_LED2, 0);
        estadoActual = RETIRAR;
        tiempoInicioEstado = millis();
      } else {  
        analogWrite(PIN_LED2, (t / (float)duracionPreparacion) * 255);
      }
      break;
    }

    case RETIRAR:   // Si se aleja, mensaje de éxito; si pasan 10 s, vuelve a esperar
      if (distanciaActual > DISTANCIA_SALIDA) {
        estadoActual = EXITO_RETIRADA;
        tiempoInicioEstado = millis();
      } else if (millis() - tiempoInicioEstado >= 10000) {
        estadoActual = ESPERANDO;
        tiempoSalidaMenu = millis();
      }
      break;

    case EXITO_RETIRADA:  // Muestra el mensaje 3 s y vuelve a esperar
      if (millis() - tiempoInicioEstado >= 3000) {
        estadoActual = ESPERANDO;
        tiempoSalidaMenu = millis();
      }
      break;

    case ADMIN: break;
  }
}
