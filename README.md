# anti keystroke deanonymization tool

kloak: *Keystroke-level online anonymization kernel*

A privacy tool that makes keystroke biometrics less effective. This
is accomplished by obfuscating the time intervals between key press and
release events, which are typically used for identification.

## Usage

There are two ways to run kloak:

  1. As an application
  2. As a Linux service

### As an application

Install dependencies:

Fedora:

    $ sudo dnf install make pkgconf-pkg-config libsodium libsodium-devel libevdev libevdev-devel

Debian:

    $ sudo apt install make pkg-config libsodium-dev libevdev-dev

First, compile `kloak` and the event capture tool `eventcap`:

    $ make all

Next, start `kloak` as root. This typically must run as root because `kloak` reads from and writes to device files:

    $ sudo ./kloak

**If you start `kloak` and lose control of your keyboard, pressing RShift + LShift + Esc will exit.** You can specify the rescue key combination with the `-k` option.

Verify that `kloak` is running by starting in verbose mode:

    $ sudo ./kloak -v
    ...
    Bufferred event at time: 1553710016364.  Type:   1,  Code:  37,  Value:   1,  Scheduled delay   84 ms
    Released event at time : 1553710016364.  Type:   1,  Code:  37,  Value:   1,  Missed target     -7 ms
    Bufferred event at time: 1553710016597.  Type:   1,  Code:  37,  Value:   0,  Scheduled delay   39 ms
    Released event at time : 1553710016597.  Type:   1,  Code:  37,  Value:   0,  Missed target     -6 ms
    Bufferred event at time: 1553710017039.  Type:   1,  Code:  32,  Value:   1,  Scheduled delay   79 ms
    Released event at time : 1553710017039.  Type:   1,  Code:  32,  Value:   1,  Missed target     -3 ms
    Bufferred event at time: 1553710017291.  Type:   1,  Code:  32,  Value:   0,  Scheduled delay   80 ms
    Bufferred event at time: 1553710017354.  Type:   1,  Code:  39,  Value:   1,  Scheduled delay   94 ms
    Lower bound raised to:   31 ms
    Released event at time : 1553710017291.  Type:   1,  Code:  32,  Value:   0,  Missed target    -33 ms
    Released event at time : 1553710017354.  Type:   1,  Code:  39,  Value:   1,  Missed target      0 ms
    ...

Notice that the lower bound on the random delay has to be raised when keys are pressed in quick succession. This ensures that the key events are written to `uinput` in the same order as they were generated.


### As a service

How to install `kloak` using apt-get

1\. Download the APT Signing Key.

```
wget https://www.whonix.org/keys/derivative.asc
```

Users can [check the Signing Key](https://www.whonix.org/wiki/Signing_Key) for better security.

2\. Add the APT Signing Key.

```
sudo cp ~/derivative.asc /usr/share/keyrings/derivative.asc
```

3\. Add the derivative repository.

```
echo "deb [signed-by=/usr/share/keyrings/derivative.asc] https://deb.whonix.org bookworm main contrib non-free" | sudo tee /etc/apt/sources.list.d/derivative.list
```

4\. Update your package lists.

```
sudo apt-get update
```

5\. Install `kloak`.

```
sudo apt-get install kloak
```

### How to build deb package

See the [Whonix package build documentation](https://www.whonix.org/wiki/Dev/Build_Documentation/security-misc). Replace the sample package name `security-misc` with `kloak` to download, build, and install kloak.

### Whonix contact and support

* [Free Forum Support](https://forums.whonix.org)
* [Professional Support](https://www.whonix.org/wiki/Professional_Support)

### Donate

`kloak` requires [donations](https://www.whonix.org/wiki/Donate) to stay alive!

### Troubleshooting

#### My keyboard seems very slow

`kloak` works by introducing a random delay to each key press and release event. This requires temporarily buffering the event before it reaches the application (e.g., a text editor).

The maximum delay is specified with the -d option. This is the maximum delay (in milliseconds) that can occur between the physical key events and writing key events to the user-level input device. The default is 100 ms, which was shown to achieve about a 20-30% reduction in identification accuracy and doesn't create too much lag between the user and the application (see the paper below). As the maximum delay increases, the ability to obfuscate typing behavior also increases and the responsiveness of the application decreases. This reflects a tradeoff between usability and privacy.

If you're a fast typist and it seems like there is a long lag between pressing a key and seeing the character on screen, try lowering the maximum delay. Alternately, if you're a slower typist, you might be able to increase the maximum delay without noticing much difference. Automatically determining the best lag for each typing speed is an item for future work.

### Options

The full usage and options are:

    $ ./kloak -h

    Usage: kloak [options]
    Options:
      -r filename: device file to read events from. Can specify multiple -r options.
      -d delay: maximum delay (milliseconds) of released events. Default 100.
      -s startup_timeout: time to wait (milliseconds) before startup. Default 100.
      -k csv_string: csv list of rescue key names to exit kloak in case the
         keyboard becomes unresponsive. Default is 'KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC'.
      -p: persistent mode (disable rescue key sequence)
      -v: verbose mode

## Try it out

See the [kloak defense testing](https://www.whonix.org/wiki/Keystroke_Deanonymization#Kloak) instructions.

## Background

`kloak` has two goals in mind:

* Make it difficult for an adversary to identify a user
* Make it difficult for an adversary to replicate a user's typing behavior

The first goal can theoretically be achieved only if all users cooperate with each other to have the same typing behavior, for example by pressing keys with exactly the same frequency. Since different users type at different speeds, this is not practical. Instead, pseudo-anonymity is achieved by obfuscating a user's typing rhythm, making it difficult for an adversary to re-identify a single user.

The second goal is to make it difficult for an adversary to forge typing behavior and impersonate a user, perhaps bypassing a two-factor authentication that uses keystroke biometrics. This is achieved by making the time between keystrokes unpredictable.

For more info, see the paper [Obfuscating Keystroke Time Intervals to Avoid Identification and Impersonation](https://arxiv.org/pdf/1609.07612.pdf).

### How it works

The time between key press and release events are typically used to identify users by their typing behavior. `kloak` obfuscates these time intervals by introducing a random delay between the physical key events and the arrival of key events at the application, for example a web browser.

`kloak` grabs the input device and writes delayed key events to the output device. Grabbing the device disables any other application from reading the events. Events are scheduled to be released in a separate thread, where a random delay is introduced before they are written to a user-level input device via `uinput`. This was inspired from [kbd-mangler](https://github.com/bgeradz/Input-Mangler/).

### When does it fail

`kloak` does not protect against all forms of keystroke biometrics that can be used for identification. Specifically,

* If the delay is too small, it is not effective. Adjust the delay to as high a value that's comfortable.
* Repeated key presses are not obfuscated. If your system is set to repeat held-down keys at a unique rate, this could leak your identity.
* Writing style is still apparent, in which [stylometry techniques could be used to determine authorship](https://vmonaco.com/papers/An%20investigation%20of%20keystroke%20and%20stylometry%20traits%20for%20authenticating%20online%20test%20takers.pdf).
* Higher level cognitive behavior, such as editing and application usage, are still apparent. These lower-frequency actions are less understood at this point, but could potentially be used to reveal identity.
