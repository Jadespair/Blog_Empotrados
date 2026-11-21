# Blog_Empotrados

Para hacer el desarrollo de la máquina expendedora, he implementado una arquitectura basándome en Multitarea Cooperativa y una Máquina de Estados Finitos (FSM).  
El objetivo principal de la práctica es cumplir con el requisito de reactividad, para lo cual hay que eliminar por completo el uso de retardos bloqueantes, los delays, en el flujo principal.  
  
A la hora de hacer el código, he utilizado la librería **ArduinoThread** para gestionar las tareas periódicas del sistema sin llegar a bloquear el procesador. Para ello he definido cuatro hilos independientes, ya que esta división hace que el sistema sea reactivo, es decir, que si, por ejemplo, el sensor DHT1 está tardando en medir, el sensor de ultrasonidos sigue funcionando y el usuario puede seguir moviendo el joystick sin que le afecte la tardanza de lectura del sensor DHT1.  
  
El primer hilo definido es el del sensor de ultrasonidos, el cual se ejecuta cada 100 ms. Este hilo mide la distancia de forma constante para detectar clientes o cuando se retire la bebida. El segundo hilo es el del sensor DHT1, el cual se ejecuta cada 2000 ms. Como el sensor DHT1 es lento por naturaleza a la hora de tomar medidas, usé un hilo para poder evitar que el sistema se congele mientras se espera la lectura. En cuanto al tercer hilo, se lo he definido al joystick, el cual se ejecuta cada 50 ms. Gracias a este hilo se nos proporciona una respuesta rápida y fluida al navegar por los menús o al modificar los precios. Y por último, el cuarto hilo es para la pantalla LCD, la cual se ejecuta cada 200 ms. Con este hilo nos encargamos de refrescar la pantalla según el estado actual del sistema.  

Una vez organizados los hilos y la dinámica del código, me centré en la gestión del botón externo, con el reset y el acceso al menú ADMIN. Para ello usé una interrupción de hardware en el pin 2 de la placa Arduino mediante **attachInterrupt(..., CHANGE)** y la rutina de servicio **isrBotonExterno()**. La interrupción captura el instante exacto en que el botón cambia de estado, guardando **millis()** de forma atómica y garantizando así una precisión absoluta, sin importar lo que esté haciendo el procesador en ese momento.  


En la práctica se nos pide implementar mecanismos de seguridad (Watchdog), por lo que lo he implementado configurándolo con un timeout de 2 segundos (**WDT0_2S**), refrescándolo periódicamente con **wdt_reset()** al inicio del **loop()**. Este mecanismo de seguridad es necesario e importante, ya que si el programa llegara a quedar bloqueado por un fallo de un sensor o un bucle infinito, el Watchdog reiniciará la placa Arduino automáticamente en 2 segundos, devolviendo el sistema a un estado seguro. También, para evitar que se produzca un bucle infinito de reinicios, he implementado una función de arranque seguro (**wdt_init**) en la sección **.inet3**.  

La segunda librería que he utilizado es **TimerOne**, la cual nos ayuda a controlar la intensidad del LED2, cuyo funcionamiento es indicar el progreso de preparación de la bebida. Esta librería utiliza el Timer1 del microcontrolador para generar una señal PWM (Pulse Width Modulation) por hardware con una resolución de 10 bits. Esto nos garantiza que el parpadeo o incremento del brillo del LED sea lineal y preciso, sin consumir ciclos de CPU.  
  

FALYA APARYADO 5















