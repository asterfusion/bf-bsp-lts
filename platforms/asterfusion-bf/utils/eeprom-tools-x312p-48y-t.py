#!/usr/bin/env python2
from __future__ import print_function

import os
import sys
import time

from subprocess import check_call, check_output, CalledProcessError

null = open(os.devnull, 'a')
availables = ['0x21', '0x22', '0x23', '0x24', '0x25', '0x26', '0x27', '0x28', '0x29', '0x2a', '0x2b', '0x2c', '0x2d', '0x2e', '0x2f', '0x30', '0x31', '0x32', '0x33', '0x34', '0xfe']

def process():
    if os.geteuid() != 0:
        raise SystemError('Root privileges are required for this operation.')
    if '--help' in sys.argv:
        print_help()
    elif '--version' in sys.argv:
        print_version()
    elif '-d' in sys.argv or '--dump' in sys.argv:
        dump_eeprom()
    elif '-s' in sys.argv or '--set' in sys.argv:
        set_eeprom()
    else:
        print_invalid()
    close_exit(0)

def dump_eeprom():
    channel = detect_channel()
    codes = [code.lower() for code in list(filter(lambda x: '0x' in x and x in availables, sys.argv[1:]))]
    print('Dumping...')
    eeprom = [
        'TLV Name            Code Len Value',
        '------------------- ---- --- -----'
    ]
    info = [
        '       Product Name\t0x21\t',
        '        Part Number\t0x22\t',
        '      Serial Number\t0x23\t',
        '   Base MAC Address\t0x24\t',
        '   Manufacture Date\t0x25\t',
        '     Device Version\t0x26\t',
        '     Label Revision\t0x27\t',
        '      Platform Name\t0x28\t',
        '       Onie Version\t0x29\t',
        '      MAC Addresses\t0x2a\t',
        '       Manufacturer\t0x2b\t',
        '       Country Code\t0x2c\t',
        '        Vendor Name\t0x2d\t',
        '       Diag Version\t0x2e\t',
        '        Service Tag\t0x2f\t',
        '      Switch Vendor\t0x30\t',
        ' Main Board Version\t0x31\t',
        '       COME Version\t0x32\t',
        'GHC-0 Board Version\t0x33\t',
        'GHC-1 Board Version\t0x34\t',
        '             CRC-32\t0xfe\t'
    ]
    if len(codes) == 0:
        eeprom += info
    else:
        for index in range(0, len(info)):
            if info[index].split('\t')[1] in codes:
                eeprom.append(info[index])
    try:
        for index in range(2, len(eeprom)):
            code = eeprom[index].split('\t')[1]
            check_call(['i2cset', '-y', channel, '0x3e', '0x01', code, '0xaa', 's'], stdout=null, stderr=null)
            if code == '0x24':
                value = check_output(['i2cdump', '-y', channel, '0x3e', 's', '0xff'], stderr=null).split('\n')[1:-1][0][7:51].strip().replace(' ', ':').upper()
                length = str(len(value.split(':')))
            elif code == '0xfe':
                value = '0x' + check_output(['i2cdump', '-y', channel, '0x3e', 's', '0xff'], stderr=null).split('\n')[1:-1][0][7:51].strip().replace(' ', '').upper()
                length = '4'
            else:
                value = ''.join(data[4:51].strip().replace(' ', '') for data in check_output(['i2cdump', '-y', channel, '0x3e', 's', '0xff'], stderr=null).split('\n')[1:-1]).decode('hex')[1:].strip('\x00')
                length = str(len(value))
            length = ' {}'.format(length) if len(length) == 2 else '  {}'.format(length)
            eeprom[index] += length + '\t' + value
    except CalledProcessError:
        close_exit('Dumping failed with wrong i2c channel.')

    for index in range(0, 2):
        print(eeprom[index])

    for index in range(2, len(eeprom)):
        data = eeprom[index].split('\t')
        print(data[0], data[1], data[2], data[3])

def set_eeprom():
    channel = detect_channel()
    collect = list(filter(lambda x: '0x' in x and '=' in x, sys.argv))
    if len(collect) == 0:
        close_exit('Nothing to set.')
    try:
        for data in collect:
            time.sleep(0.2)
            code = data.split('=')[0].lower()
            content = data.split('=', 1)[1]
            if len(content) > 19:
                print('Skipping code {} due to overlength...'.format(code))
                continue
            if content.startswith('"') and content.endswith('"'):
                content = content.strip('"')
            if code not in availables or code == '0xfe':
                print('Skipping invalid code {}...'.format(code))
                continue
            elif code == '0x24':
                value = ['0x' + x for x in content.split(':')]
                length = '0x' + str(len(value)) if len(str(len(value))) == 2 else '0x0' + str(len(value))
                try:
                    assert length == '0x06'
                except AssertionError:
                    print('Skipping 0x24 due to invalid mac address.')
                    continue
            else:
                value = ['0x' + x.encode('hex') if len(x.encode('hex')) == 2 else '0x0' + x.encode('hex') for x in content]
                length = '0x' + str(len(value)) if len(str(len(value))) == 2 else '0x0' + str(len(value))
            cmd = ['i2cset', '-y', channel, '0x3e', '0x02', code, length] + value + ['s']
            print('Setting {} to {}'.format(code, content))
            check_call(args=cmd, stdout=null, stderr=null)
    except CalledProcessError:
        close_exit('Setting failed with wrong i2c channel.')
    else:
        close_exit('A SOFT REBOOT is required for the device to save changes.')

def print_help():
    print('Description:')
    print('  A script helpful for dumping or setting eeprom data of X312P-T.')
    print('')
    print('Usage: [python] {} [OPTION] [CHANNEL] [CODE][=VALUE]...'.format(sys.argv[0]))
    print('  Available options are listed below:')
    print('  -d, --dump')
    print('      Dump specified eeprom data with one or more tlv code')
    print('      Dump all eeprom data with no tlv code')
    print('      I2c channel will be automatically detected if not specified')
    print('  -s, --set')
    print('      Set eeprom data with with entry in format code=value')
    print('      Multiple values can be set at once with multiple entries')
    print('      I2c channel will be automatically detected if not specified')
    print('  -h, --help')
    print('      Display help docs and exit')
    print('  -v, --version')
    print('      Display version info and exit')
    print('')
    print('Examples for function dump or set :')
    print('  Dump with specified channel:     {} -d 1 0x21 0x26 0x27'.format(sys.argv[0]))
    print('  Set with specified channel:      {} -s 1 0x21=X312P-T 0x24=00:11:22:33:44:55 0x25="03/09/2023 16:59:59"'.format(sys.argv[0]))
    print('  Dump with auto-detected channel: {} -d 0x21 0x26 0x27'.format(sys.argv[0]))
    print('  Set with auto-detected channel:  {} -s 0x21=X312P-T 0x24=00:11:22:33:44:55 0x25="03/09/2023 16:59:59"'.format(sys.argv[0]))

def print_version():
    print('{} 1.0.4'.format(os.path.basename(sys.argv[0])))
    print('')
    print('License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.')
    print('This is free software: you are free to change and redistribute it.')
    print('There is NO WARRANTY, to the extent permitted by law.')
    print('')
    print('Written by SunZheng from Asterfusion Wuhan.')
    print('')
    print('Change log:')
    print('1.0.0')
    print('  Implement basical dumping & setting function.')
    print('')
    print('1.0.1')
    print('  Remove delay on dumping eeprom.')
    print('  Fix a problem of setting invalid mac address.')
    print('')
    print('1.0.2')
    print('  Fix dumping mac address displays corrupted characters.')
    print('  Display changelog when print version.')
    print('')
    print('1.0.3')
    print('  Fix dumping CRC-32 displays corrupted characters.')
    print('  Fix a problem of displaying wrong length.')
    print('  Fix a problem of multiple setting at once always fails.')
    print('')
    print('1.0.4')
    print('  Add length limitation.')
    print('  Fix mac address displaying in lowercase.')
    print('  Fix reporting error on failing in detecting channels.')
    print('  Optimize code and beautify displaying messages.')
    print('')
    print('1.0.5')
    print('  Fix unable to set eeprom with value that includes spaces.')
    print('  Fix runtime error when permission is insufficient.')
    print('')
    print('1.0.6')
    print('  Standardize eeprom field names.')
    print('  Add example for eeprom field with space in its value.')
    print('  Add `REBOOT` notification after setting eeprom successfully.')
    print('  TODO: Add Python3 compatibility.')

def print_invalid():
    print('{}: invalid usage'.format(os.path.basename(sys.argv[0])))
    print('Try \'{} --help\' for more information.'.format(sys.argv[0]))

def detect_channel():
    channels = list(filter(lambda x: '0x' not in x and '-d' not in x and '--dump' not in x and '-s' not in x and '--set' not in x, sys.argv[1:]))
    if len(channels) == 0:
        detected = check_output(['i2cdetect', '-l'], stderr=null).split('\n')
        print('No i2c channel provided, start auto detecting i2c channel.')
        # Detecting CP2112 channel
        try:
            cp2112 = list(filter(lambda x: 'CP2112' in x, detected))[0][4]
        except IndexError:
            cp2112 = False
            print('Detecting CP2112 channel failed.')
        # Detecting SuperIO channel
        try:
            superio = list(filter(lambda x: 'sio_smbus' in x, detected))[0][4]
        except IndexError:
            superio = False
            print('Detecting SuperIO channel failed.')
        # Immediately exit if both not detected
        if not cp2112 and not superio:
            close_exit('Detecting i2c channel failed, you need to manually provide it by passing an argument.')
        # Try CP2112 channel
        try:
            channel = cp2112
            check_call(['i2cset', '-y', cp2112, '0x3e', '0x01', '0x21', '0xaa', 's'], stdout=null, stderr=null)
            print('Trying to use i2c channel {} succeeded.'.format(cp2112))
        except CalledProcessError:
            # Try SuperIO channel
            channel = superio
            check_call(['i2cset', '-y', superio, '0x3e', '0x01', '0x21', '0xaa', 's'], stdout=null, stderr=null)
            print('Trying to use i2c channel {} succeeded.'.format(superio))
        except:
            close_exit('Detecting failed.')
    else:
        channel = channels[0]
        print('Using user-provided i2c channel {}.'.format(channel))
    return channel

def close_exit(text):
    null.close()
    exit(text)

if __name__ == '__main__':
    try:
        process()
    except SystemError as e:
        close_exit(str(e))
    except KeyboardInterrupt:
        close_exit('\rUser manually interrupted.')