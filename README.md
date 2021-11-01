# estoraged

This daemon serves as an abstraction for an encrypted storage device,
encapsulating the security functionality and providing a D-Bus interface to
manage the encrypted filesystem on the device. Using the D-Bus interface, other
software components can interact with eStoraged to do things like create a
new encrypted filesystem, wipe its contents, lock/unlock the device, or change
the password.
