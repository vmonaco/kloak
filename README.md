# anti keystroke deanonymization tool #

A keystroke-level online anonymization kernel.

A privacy tool that makes keystroke biometrics less effective. This
is accomplished by obfuscating the time intervals between key press and
release events, which are typically used for identification.
## How to install `kloak` using apt-get ##

1\. Add [Whonix's Signing Key](https://www.whonix.org/wiki/Whonix_Signing_Key).

```
sudo apt-key --keyring /etc/apt/trusted.gpg.d/whonix.gpg adv --keyserver hkp://ipv4.pool.sks-keyservers.net:80 --recv-keys 916B8D99C38EAF5E8ADC7A2A8D66066A2EEACCDA
```

3\. Add Whonix's APT repository.

```
echo "deb http://deb.whonix.org buster main contrib non-free" | sudo tee /etc/apt/sources.list.d/whonix.list
```

4\. Update your package lists.

```
sudo apt-get update
```

5\. Install `kloak`.

```
sudo apt-get install kloak
```

## How to Build deb Package ##

Replace `apparmor-profile-torbrowser` with the actual name of this package with `kloak` and see [instructions](https://www.whonix.org/wiki/Dev/Build_Documentation/apparmor-profile-torbrowser).

## Contact ##

* [Free Forum Support](https://forums.whonix.org)
* [Professional Support](https://www.whonix.org/wiki/Professional_Support)

## Donate ##

`kloak` requires [donations](https://www.whonix.org/wiki/Donate) to stay alive!
