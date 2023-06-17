# Baja Solar ENG 100L
 UCSD Baja Solar

We utilize an RTOS (Real-Time Operating System) to run our water temperature control system. It integrates various hardware components, including temperature sensors, a flow sensor, a pump, a joystick, and an OLED display. The code initializes the required libraries and defines the necessary pins for these components. It creates two instances of the OneWire library to communicate with two Temperature sensors, and establishes task queues for temperature readings and flow rate values.
