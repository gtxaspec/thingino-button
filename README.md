# Thingino-Button

`thingino-button` is a simple program designed to monitor input events from a specified device and execute corresponding commands based on the configuration. It is primarily aimed at embedded systems where specific actions need to be triggered by pressing, releasing, or holding buttons.

## Features

- Monitor an input device for key events.
- Execute different commands on key press, release, or timed hold.
- Configurable through a simple configuration file.
- Supports running as a daemon or in silent mode.
- Basic key support for `KEY_ENTER` and keys `0-9`. More keys can be easily added by modifying the source code.

## Requirements

- A Linux-based system with access to the input device (e.g., `/dev/input/event0`).

## Installation

1. **Clone the Repository:**
   ```sh
   git clone https://github.com/gtxaspec/thingino-button.git
   cd thingino-button
   ```

2. **Compile the Program:**
   To compile the program, you can use the provided Makefile. If you are cross-compiling, set the `CROSS_COMPILE` variable.
   ```sh
   make CROSS_COMPILE=mipsel-linux-
   ```

   This will generate the `thingino-button` executable.

## Usage

1. **Create a Configuration File:**

   Create a file named `/etc/thingino-button.conf` with the following format:
   ```ini
   # Device to monitor
   DEVICE=/dev/input/event0

   # Key action mappings
   # Format: KEY_<key_name> <action> <command> [<time>]
   KEY_ENTER PRESS /bin/command1
   KEY_ENTER RELEASE /bin/command2 
   KEY_ENTER TIMED /bin/command-timed press 0.1
   KEY_ENTER TIMED /bin/command-timed 3
   KEY_ENTER TIMED /bin/command-timed 5
   KEY_ENTER TIMED /bin/command-timed 20
   ```

2. **Run the Program:**

   You can run the program directly or with optional flags:
   ```sh
   ./thingino-button [-s] [-d] [input_device]
   ```

   - `-s`: Run in silent mode, logging to syslog.
   - `-d`: Run as a daemon, logging to syslog.
   - `input_device`: Optional input device path, overrides the config file.

   Example:
   ```sh
   ./thingino-button -s /dev/input/event1
   ```

## Configuration File Format

The configuration file should be located at `/etc/thingino-button.conf` and follows this format:

```ini
# Device to be monitored for input events
DEVICE=/dev/input/event0

# Key action mappings
# Format: KEY_<key_name> <action> <command> [<time>]
# - <key_name>: Name of the key (e.g., ENTER, 1, 2, etc.)
# - <action>: Type of action (PRESS, RELEASE, TIMED)
# - <command>: Command to execute when the action occurs
# - [<time>]: (Optional) Time in seconds for TIMED actions

# Example mappings:
KEY_ENTER PRESS /bin/command1
KEY_ENTER RELEASE /bin/command2
KEY_ENTER TIMED /bin/command-timed press 0.1
KEY_ENTER TIMED /bin/command-timed 3
KEY_ENTER TIMED /bin/command-timed 5
KEY_ENTER TIMED /bin/command-timed 20
```

## Contributing

Contributions are welcome! Please fork the repository and submit a pull request for any enhancements or bug fixes.

## License

This project is licensed under the MIT License.
