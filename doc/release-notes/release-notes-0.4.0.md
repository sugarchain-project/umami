Sugarchain version 0.4.0 is now available for download at:
http://sourceforge.net/projects/sugarchain/files/Sugarchain/sugarchain-0.4.0/

The main feature in this release is wallet private key encryption;
you can set a passphrase that must be entered before sending coins.
See below for more information; if you decide to encrypt your wallet,
WRITE DOWN YOUR PASSPHRASE AND PUT IT IN A SECURE LOCATION. If you
forget or lose your wallet passphrase, you lose your sugarchains.
Previous versions of sugarchain are unable to read encrypted wallets,
and will crash on startup if the wallet is encrypted.

Also note: sugarchain version 0.4 uses a newer version of Berkeley DB
(bdb version 4.8) than previous versions (bdb 4.7). If you upgrade
to version 0.4 and then revert back to an earlier version of sugarchain
the it may be unable to start because bdb 4.7 cannot read bdb 4.8
"log" files.


Notable bug fixes from version 0.3.24:

Fix several sugarchain-becomes-unresponsive bugs due to multithreading
deadlocks.

Optimize database writes for large (lots of inputs) transactions
(fixes a potential denial-of-service attack)


Wallet Encryption

Sugarchain supports native wallet encryption so that people who steal your
wallet file don't automatically get access to all of your Sugarchains.
In order to enable this feature, choose "Encrypt Wallet" from the
Options menu.  You will be prompted to enter a passphrase, which
will be used as the key to encrypt your wallet and will be needed
every time you wish to send Sugarchains.  If you lose this passphrase,
you will lose access to spend all of the sugarchains in your wallet,
no one, not even the Sugarchain developers can recover your Sugarchains.
This means you are responsible for your own security, store your
passphrase in a secure location and do not forget it.

Remember that the encryption built into sugarchain only encrypts the
actual keys which are required to send your sugarchains, not the full
wallet.  This means that someone who steals your wallet file will
be able to see all the addresses which belong to you, as well as the
relevant transactions, you are only protected from someone spending
your coins.

It is recommended that you backup your wallet file before you
encrypt your wallet.  To do this, close the Sugarchain client and
copy the wallet.dat file from ~/.sugarchain/ on Linux, /Users/(user
name)/Application Support/Sugarchain/ on Mac OSX, and %APPDATA%/Sugarchain/
on Windows (that is /Users/(user name)/AppData/Roaming/Sugarchain on
Windows Vista and 7 and /Documents and Settings/(user name)/Application
Data/Sugarchain on Windows XP).  Once you have copied that file to a
safe location, reopen the Sugarchain client and Encrypt your wallet.
If everything goes fine, delete the backup and enjoy your encrypted
wallet.  Note that once you encrypt your wallet, you will never be
able to go back to a version of the Sugarchain client older than 0.4.

Keep in mind that you are always responsible for your own security.
All it takes is a slightly more advanced wallet-stealing trojan which
installs a keylogger to steal your wallet passphrase as you enter it
in addition to your wallet file and you have lost all your Sugarchains.
Wallet encryption cannot keep you safe if you do not practice
good security, such as running up-to-date antivirus software, only
entering your wallet passphrase in the Sugarchain client and using the
same passphrase only as your wallet passphrase.

See the doc/README file in the sugarchain source for technical details
of wallet encryption.
