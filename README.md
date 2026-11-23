# Blog_Empotrados

Para hacer el desarrollo de la máquina expendedora, he implementado una arquitectura basándome en Multitarea Cooperativa y una Máquina de Estados Finitos (FSM).
El objetivo principal de la práctica es cumplir con el requisito de reactividad, para lo cual hay que eliminar por completo el uso de retardos bloqueantes, los delays, en el flujo principal.  
  
A la hora de hacer el código, he utilizado la librería `ArduinoThread` para gestionar las tareas periódicas del sistema sin llegar a bloquear el procesador. Para ello he definido cuatro hilos independientes, ya que esta división hace que el sistema sea reactivo, es decir, que si, por ejemplo, el sensor DHT11 está tardando en medir, el sensor de ultrasonidos sigue funcionando y el usuario puede seguir moviendo el joystick sin que le afecte la tardanza de lectura del sensor DHT11.  

El primer hilo definido es el del **sensor de ultrasonidos**, el cual se ejecuta cada 100 ms. Este hilo mide la distancia de forma constante para detectar clientes o cuando se retire la bebida. El segundo hilo es el del **sensor DHT11**, el cual se ejecuta cada 2000 ms. Como el sensor DHT11 es lento por naturaleza a la hora de tomar medidas, usé un hilo para poder evitar que el sistema se congele mientras se espera la lectura. En cuanto al tercer hilo, lo he definido para el **joystick**, el cual se ejecuta cada 50 ms. Gracias a este hilo se nos proporciona una respuesta rápida y fluida al navegar por los menús o al modificar los precios. Y por último, el cuarto hilo es para la **pantalla LCD**, la cual se ejecuta cada 200 ms. Con este hilo nos encargamos de refrescar la pantalla según el estado actual del sistema.  
  
Una vez organizados los hilos y la dinámica del código, me centré en la gestión del botón externo, con el reset y el acceso al `menú ADMIN`. Para ello usé una **interrupción de hardware** en el pin 2 de la placa Arduino mediante `attachInterrupt(..., CHANGE)` y la rutina de servicio `isrBotonExterno()`. La interrupción captura el instante exacto en que el botón cambia de estado, guardando `millis()` de forma atómica y garantizando así una precisión absoluta, sin importar lo que esté haciendo el procesador en ese momento.  
  
En la práctica se nos pide implementar mecanismos de seguridad (**Watchdog**), por lo que lo he implementado configurándolo con un timeout de 2 segundos (`WDTO_2S`), refrescándolo periódicamente con `wdt_reset()` al inicio del `loop()`. Este mecanismo de seguridad es necesario e importante, ya que si el programa llegara a quedar bloqueado por un fallo de un sensor o un bucle infinito, el **Watchdog** reiniciará la placa Arduino automáticamente en 2 segundos, devolviendo el sistema a un estado seguro. También, para evitar que se produzca un bucle infinito de reinicios, he implementado una función de arranque seguro (`wdt_init`) en la sección `.init3`.  
  

Al no poder utilizar `delay()` en el flujo principal, para no bloquear el `setup()` con un delay, he añadido un estado `ARRANCANDO` a la FSM. Luego en el `loop()` calculo el tiempo transcurrido con `millis()` y aplico `digitalWrite(PIN_LED1, (t / 1000) % 2 == 0)`, el cual genera un parpadeo de un segundo de duración, sin bloquear. Pasados 6 segundos se pasa automáticamente a `ESPERANDO`. También, para evitar el uso de `delay()` en el **antirrebote del joystick**, uso una variable `botonSuelto` para poder garantizar que solo se registre un clic por cada pulsación, junto con la comprobación `millis() - t_click > 200` para evitar los rebotes mecánicos. Aunque he evitado usar `delay()`, para el **sensor de ultrasonidos**, por requisito, se usa `delayMicroseconds(10)` para generar el pulso de trigger de 10 microsegundos.  
  
Durante la práctica he tenido un problema intentando implementar la librería `TimerOne`. Inicialmente implementé el **PWM** del LED2 con `TimerOne`, pero durante el proceso vi que `TimerOne` utiliza el **Timer1** en los pines 9 y 10. Una vez implementado vi que la señal **PWM** dejaba de generarse y el LED2 ya no se iluminaba. Tras descartar problemas de conectividad y hardware, ya que el LED se encendía bien anteriormente, opté por usar `analogWrite()` en su lugar. A pesar de que es una configuración más robusta frente a las interrupciones del resto del sistema, ambos usan el **Timer1**. Usando `analogWrite()` consigo el mismo resultado, aunque no he podido aplicar la librería `TimerOne` ya que no he encontrado una solución al problema.  
  
La lógica de negocio se ha centralizado en una **máquina de estados** implementada con un `enum` (`ESPERANDO`, `BIENVENIDA`, `MENU`, `PREPARANDO`, `RETIRAR`, `EXITO_RETIRADA`, `ADMIN`). Esta máquina de estados nos permite estructurar el código de una forma modular. En el `loop()` principal solo se comprueba en qué estado se está y si debe transicionar hacia otro. Para las **transiciones** que requieren tiempo, como los 5 segundos de bienvenida, los 3 segundos de retirada o el tiempo aleatorio de preparación, se utiliza la función `millis()`, que devuelve el tiempo en milisegundos desde que arrancó la placa. Esto nos permite crear esperas no bloqueantes, donde el sistema comprueba si ha pasado el tiempo pero, mientras tanto, sigue ejecutando los hilos y leyendo sensores.  
  







<img width="3619" height="2246" alt="circuito_empotrados_bb" src="https://github.com/user-attachments/assets/c972e1e7-e681-4eb5-8c77-1ddc67b18968" />  
  


<img width="4080" height="3060" alt="IMG_20261120_194210" src="https://github.com/user-attachments/assets/2fe5a993-b6a6-4865-8fe1-edea639c4919" />  
  

<img width="4080" height="3060" alt="_storage_emulated_0_DCIM_Camera_IMG_20261120_194214" src="https://github.com/user-attachments/assets/d3fdbc61-c6ec-47cd-b3d1-9187b8a3b2bd" />


