# M5Dial MQTT Fan Speed Controller

This project is a basic MQTT fan speed controller with inside and outside temperature display via MQTT topics, designed for the M5Dial device.
It uses WiFi and MQTT for communication and displays information on the M5Dial screen.
This is a quick conveniance hack.

## Features

- Connects to a WiFi network.
- Subscribes to MQTT topics to receive temperature data.
- Displays fan speed and temperature data on the M5Dial screen.
- Updates fan speed via MQTT messages.
- Provides local and remote updates for fan speed.
- Dynamically adjusts the display based on fan speed percentage.

## Hardware Requirements

- M5Dial device
- WiFi network
- MQTT broker

## Software Requirements

- Arduino IDE
- M5Stack libraries
- PubSubClient library

## License

This project is open source and available under the [MIT License](LICENSE).