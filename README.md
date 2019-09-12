# anti keystroke deanonymization tool #

kloak: *Keystroke-level online anonymization kernel*

A privacy tool that makes keystroke biometrics less effective. This
is accomplished by obfuscating the time intervals between key press and
release events, which are typically used for identification.

## Usage

There are two ways to run kloak:

  1. As an application
  2. As a Linux service

### As an application

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

1\. Download [Whonix's Signing Key]().

```
wget https://www.whonix.org/patrick.asc
```

Users can [check Whonix Signing Key](https://www.whonix.org/wiki/Whonix_Signing_Key) for better security.

2\. Add Whonix's signing key.

```
sudo apt-key --keyring /etc/apt/trusted.gpg.d/whonix.gpg add ~/patrick.asc
```

3\. Add Whonix's APT repository.

```
echo "deb https://deb.whonix.org buster main contrib non-free" | sudo tee /etc/apt/sources.list.d/whonix.list
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

Replace `apparmor-profile-torbrowser` with the actual name of this package with `kloak` and see [instructions](https://www.whonix.org/wiki/Dev/Build_Documentation/apparmor-profile-torbrowser).

### Whonix contact and support

* [Free Forum Support](https://forums.whonix.org)
* [Professional Support](https://www.whonix.org/wiki/Professional_Support)

### Donate

`kloak` requires [donations](https://www.whonix.org/wiki/Donate) to stay alive!


### Troubleshooting

#### Can't open input/output device

`kloak` will attempt to find your keyboard device to read events from and the location of `uinput` to write events to. If `kloak` cannot find either the input device or output device, these must be specified with the `-r` and `-w` options, respectively.

To find the keyboard device for reading events: determine which device file corresponds to the physical keyboard. Use `eventcap` (or some other event capture tool) and look for the device that generates events when keys are pressed. This will typically be one of `/dev/input/event[0-7]`. In this example, it's `/dev/input/event4`:

    $ sudo ./eventcap /dev/input/event4
    Reading From : /dev/input/event4 (AT Translated Set 2 keyboard)
    Type:   4    Code:   4    Value:  15
    Type:   1    Code:  15    Value:   0
    Type:   0    Code:   0    Value:   0
    Type:   4    Code:   4    Value:  56
    Type:   1    Code:  56    Value:   0
    Type:   0    Code:   0    Value:   0

`uinput` is the [kernel module](http://thiemonge.org/getting-started-with-uinput) that allows user-land applications to create input devices. This is typically located at either `/dev/uinput` or `/dev/input/uinput`.

Start `kloak` by specifying the input and output device files:

    $ sudo ./kloak -r /dev/input/event4 -w /dev/uinput

#### My keyboard seems very slow

`kloak` works by introducing a random delay to each key press and release event. This requires temporarily buffering the event before it reaches the application (e.g., a text editor).

The maximum delay is specified with the -d option. This is the maximum delay (in milliseconds) that can occur between the physical key events and writing key events to the user-level input device. The default is 100 ms, which was shown to achieve about a 20-30% reduction in identification accuracy and doesn't create too much lag between the user and the application (see the paper below). As the maximum delay increases, the ability to obfuscate typing behavior also increases and the responsive of the application decreases. This reflects a tradeoff between usability and privacy.

If you're a fast typist and it seems like there is a long lag between pressing a key and seeing the character on screen, try lowering the maximum delay. Alternately, if you're a slower typist, you might be able to increase the maximum delay without noticing much difference. Automatically determining the best lag for each typing speed is an item for future work.

### Options

The full usage and options are:

    $ ./kloak -h

    Usage: kloak [options]
    Options:
      -r filename: device file to read events from
      -w filename: device file to write events to (should be uinput)
      -d delay: maximum delay (milliseconds) of released events. Default 100.
      -s startup_timeout: time to wait (milliseconds) before startup. Default 100.
      -k csv_string: csv list of rescue key names to exit kloak in case the
         keyboard becomes unresponsive. Default is 'KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_ESC'.
      -v: verbose mode

## Try it out

You can test that `kloak` actually works by trying an [online keystroke biometrics demo](https://www.keytrac.net/en/tryout). For example, try these three different scenarios:
* Train normal, test normal
* Train normal, test kloak
* Train `kloak`, test `kloak`

*Train normal* means to train with normal typing behavior, i.e., without `kloak` running. At the enrollment page on the KeyTrac demo, enter a username and password without `kloak` running, and then on the authenticate page, try authenticating. For example, the train normal/test normal result is:

<div align="center">
  <img src="figures/train-normal_test-normal.png"><br><br>
</div>

Start `kloak` and then try authenticating again. These results were obtained using a maximum delay of 200 ms (`-d 200`). The train normal/test `kloak` result is:

<div align="center">
  <img src="figures/train-normal_test-kloak.png"><br><br>
</div>

Now go back to the [enrollment page](https://www.keytrac.net/en/tryout). Enroll With `kloak` running and then try authenticating with `kloak` still running. Again, this is with a 200 ms maximum delay. The train `kloak`/test `kloak` result is:

<div align="center">
  <img src="figures/train-kloak_test-kloak.png"><br><br>
</div>

Your results may differ, especially in the train `kloak`/test `kloak` scenario. The train `kloak`/test `kloak` scenario is more difficult to anonymize than the train normal/test `kloak` scenario. This is because *kloak obfuscates your typing behavior, but does not make your typing behavior similar to other users*. This dilemma relates to the problem of user cooperation. It's easy to make your typing behavior look like something that it's not, but what should that be? If it's too unique, then the change does more harm then good, allowing you to be easily identified. Without the cooperation of other users, it's difficult to choose a behavior that's hard to distinguish.

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
* Writing style is still apparent, in which [stylometry techniques could be used to determine authorship](http://www.vmonaco.com/publications/An%20investigation%20of%20keystroke%20and%20stylometry%20traits%20for%20authenticating%20online%20test%20takers.pdf).
* Higher level cognitive behavior, such as editing and application usage, are still apparent. These lower-frequency actions are less understood at this point, but could potentially be used to reveal identity.
