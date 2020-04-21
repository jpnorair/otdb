#! python3

import otdb
import argparse
import sys

def printf(fmt, *params):
    sys.stdout.write(fmt % params)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="otdb module testbed")
    parser.add_argument('--socket', default='/opt/otdb/otdb.sock', help="OTDB Socket")

    args = parser.parse_args()

    cli = otdb(str(args.socket))

    if cli.connect():
        printf("PASS: connected to %s\n", str(args.socket))

        device_list = cli.listdevices()
        printf("cli.listdevices: %s\n", str(device_list))

        output = cli.f_read(uid=device_list[0], file=0)
        if output is not None:
            print("PASS: cli.f_read()")
            print(output)
        else:
            print("FAIL: cli.f_read()")

        output = cli.f_readall(uid=device_list[0], file=0)
        if output is not None:
            print("PASS: cli.f_readall()")
            print(output)
        else:
            print("FAIL: cli.f_readall()")

        # Finish the test by closing the connection
        cli.disconnect()

    else:
        printf("FAIL: Could not connect to %s\n", str(args.socket))

    sys.exit(0)

