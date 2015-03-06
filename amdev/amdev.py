#!/usr/bin/env python
"""Amiigo Device helper

Copyright Amiigo Inc.
"""
from __future__ import print_function
import sys
import argparse
import time
import dbus
from dbus.mainloop.glib import DBusGMainLoop
import gobject
import logging
import re
import os
from subprocess import Popen, PIPE, STDOUT
import signal
from threading import Thread

try:
    from Queue import Queue, Empty
except ImportError:
    from queue import Queue, Empty  # python 3.x

import datetime    

ON_POSIX = 'posix' in sys.builtin_module_names


def enqueue_output(out, queue):
    for line in iter(out.readline, b''):
        queue.put(line)
    out.close()

if __name__ == "__main__":
    logging.basicConfig()
    
logger = logging.getLogger(__name__)

BUS_NAME = 'org.bluez'
SERVICE_NAME = 'org.bluez'
ADAPTER_INTERFACE = SERVICE_NAME + '.Adapter1'
DEVICE_INTERFACE = SERVICE_NAME + '.Device1'

__version__ = 1.6


class AmiigoDevice():
    def __init__(self, force_amlink=False, force=False, adapter=''):
        self.done = False
        self.force_amlink = force_amlink
        self.force = force
        self.amiigos = {}
        self.timeout_secs = 10
        self.max_timeout = 30
        self.adapter = adapter

        if self.force_amlink:
            self._manager = None
        else:
            DBusGMainLoop(set_as_default=True)
            self._loop = gobject.MainLoop()
            self._bus = dbus.SystemBus()
    
            man = self._bus.get_object(BUS_NAME, '/')
    
            try:
                self._manager = dbus.Interface(man, 'org.freedesktop.DBus.ObjectManager')
                self._manager.GetManagedObjects()
            except:
                self._manager = None      
                          
        self.cmds_base = ['amlink']
        if self.adapter:
            self.cmds_base = self.cmds_base + ['-i', self.adapter]

    def _keep_status(self, dut, stdout):
        logs = 0
        battery = 0
        charging = False
        recording = False
        version = ''
        stdout = stdout.rstrip().lower()
        if '(Rec)' in stdout:
            recording = True
        if 'charging' in stdout:
            charging = True
        if 'version:' in stdout:
            m = re.findall('version:\s(?P<ver>[.|0-9]+)\s', stdout)
            if m:
                version = m[0]
        if 'logs:' in stdout:
            m = re.findall('logs:\s(?P<logs>[0-9]+)\s', stdout)
            if m:
                logs = m[0]
        if 'battery:' in stdout:
            m = re.findall('battery:\s(?P<battery>[0-9]+)\%\s', stdout)
            if m:
                battery = m[0]
        return charging, battery, recording, logs, version

    def test(self, dut):
        """test blink
        """
        
        charging = False
        
        passed = False

        cmds = self.cmds_base + ['--b', dut]

        cmds = cmds + ['--c', 'blink']
        logger.info(cmds)
        p = Popen(cmds, stdout=PIPE, stderr=PIPE, close_fds=ON_POSIX, preexec_fn=os.setsid)
        stdout, stderr = p.communicate()
        if stderr:
            logger.debug({'stdout': stdout, 'stderr': stderr})
        elif not stdout:
            print('no output')
        else:
            passed = True
            stdout = stdout.rstrip().lower() 
            if 'charging' in stdout:
                charging = True
        
        if not passed:
            print('\tblink test did not pass, try again or change device')

        sys.stdout.flush()
        return passed, charging
    
    def discover_amlink(self, console=False):
        """discover using amlink
        """
        
        pattern = '(?P<address>([0-9A-F]{2}:){5}[0-9A-F]{2})\s+(?:(?P<name>.{13}[W|S][0-9A-F]{6})|\(unknown\))(?:\s+(?P<uuid>[0-9A-F]{32}))?$'
        
        p = Popen(['amlink', '--lescan'], stdout=PIPE, stderr=STDOUT, bufsize=1, close_fds=ON_POSIX, preexec_fn=os.setsid)

        def _process():

            def _process_line(line):
                m = re.match(pattern, line)
                if m:
                    m = m.groupdict()
                    address = m.get('address', '').upper()
                    name = m.get('name', '')
                    uuid = m.get('uuid', '')
                    # if name is Amiigo-ish or if we have amiigo service uuid
                    if name or uuid == 'CCA3100078C647859E450887D451317C':
                        self.add_amiigo(address, name, console=console)
            q = Queue()
            t = Thread(target=enqueue_output, args=(p.stdout, q))
            t.daemon = True  # thread dies with the program
            t.start()
            
            _time = time.time()
    
            elapsed = 0
            done = False
            while not done:
                try:  
                    line = q.get(timeout=.1)
                except Empty:
                    line = ''
                if line:
                    line = line.rstrip()
                    _process_line(line)
                elapsed = time.time() - _time
                done = elapsed > self.timeout_secs or self.done
        
        try:
            _process()
        except:
            pass

        try:            
            os.kill(p.pid, signal.SIGINT)
        except:
            pass
        
        try:
            p.wait()
        except (KeyboardInterrupt, SystemExit):
            try:
                os.kill(p.pid, signal.SIGKILL)
            except:
                pass
        except:
            pass
        

    def is_amiigo(self, name):
        """check name ot see if it is amiigo
        """
        is_amiigo = re.match('.{13}[W|S][0-9A-F]{6}$', name) is not None
        # print('test', name, is_amiigo)
        return is_amiigo

    def add_amiigo(self, address, name, rssi=0, console=False):
        """do something with amiigo
        """
        if name is None:
            name = ''
        amiigo_type = self.amiigos.get(address, {}).get('type', '')
        if len(name) > 7:
            if name[-7] == 'S':
                amiigo_type = 'Shoepod'
            elif name[-7] == 'W':
                amiigo_type = 'Wristband'

        if not name:
            name = self.amiigos.get(address, {}).get('name', '(unknown)')
        
        rssi = int(rssi)
        amiigo = self.amiigos.get(address)
        new_amiigo = (amiigo is None or
                      amiigo.get('type') != amiigo_type or
                      amiigo.get('rssi') != rssi or
                      amiigo.get('name') != name)
            
        self.amiigos[address] = {'dut': '',
                                 'passed': False,
                                 'bad z': False, 
                                 'type': amiigo_type, 
                                 'rssi': rssi, 
                                 'name': name} 
        if console and new_amiigo:
            msg = 'Address: %s, RSSI: %s, Name: %s' % (address, rssi, name)
            if amiigo_type:
                msg = msg + ' %s' % amiigo_type
            print(msg)
        
        sys.stdout.flush()
    
    def _wait_start_discovery(self, adapter, timeout_secs):
        bus = self._bus
        device_prop = dbus.Interface(bus.get_object(BUS_NAME, adapter.object_path),
                                     'org.freedesktop.DBus.Properties')
        
        _time = time.time()
        while time.time() - _time < timeout_secs:
            prop = device_prop.Get(ADAPTER_INTERFACE, 'Discovering')
            if prop == 1:
                break
            time.sleep(1)
    
    def _switch_on_adapter(self, adapter, on):
        bus = self._bus
        device_prop = dbus.Interface(bus.get_object(BUS_NAME, adapter.object_path),
                                     'org.freedesktop.DBus.Properties')
        device_prop.Set(ADAPTER_INTERFACE, 'Powered', on)
        
    def discover(self, console=False):
        if self._manager is None or self.force_amlink:
            return self.discover_amlink(console=console)
        disc = self.discover_dbus(console=console)
        time.sleep(3)
        return disc

    def discover_dbus(self, console=False):
        bus = self._bus
        objects = self._manager.GetManagedObjects()
        adapters = []
        for path, interfaces in objects.iteritems():
            adapter = interfaces.get(ADAPTER_INTERFACE)
            if adapter is None:
                continue
            obj = bus.get_object(BUS_NAME, path)
            adapters.append(dbus.Interface(obj, ADAPTER_INTERFACE))
        
        if len(adapters) == 0:
            raise RuntimeError('No BLE adapter found')
        
        adapter = adapters[0]
        if self.adapter:
            try:
                adapters = [a for a in adapters if self.adapter in a.object_path]
                if len(adapters):
                    adapter = adapters[0]
            except:
                pass
            
        devices = dict()
        if self.force:
            # turn off adapter first
            self._switch_on_adapter(adapter, False)
        self._switch_on_adapter(adapter, True)

        def _scan_timeout():
            adapter.StopDiscovery()
            self._loop.quit()
            return False
        
        def _interface_added(path, interfaces):
            path = unicode(path)
            if DEVICE_INTERFACE not in interfaces:
                return
            properties = dict(interfaces[DEVICE_INTERFACE])
            if path in devices:
                logger.info('replace old device properties with new device properties')
            devices[path] = properties
            address = (unicode(properties['Address']) if 'Address' in properties else '<unknown>')
            logger.info('Bluetooth Device Found: %s.', address)
            is_amiigo = False
            name = ''
            for key, value in properties.iteritems():
                logger.info('\t%s : %s', key, value)
                if 'UUID' in key and value == 'cca31000-78c6-4785-9e45-0887d451317c':
                    is_amiigo = True
                if key == 'Name' or (key == 'Alias' and not name):
                    name = unicode(value)
                    is_amiigo = is_amiigo or self.is_amiigo(name)
            properties['is_amiigo'] = is_amiigo
            rssi = int(properties.get('RSSI', 0))
            msg = 'Address: %s, RSSI: %s, Name: %s' % (address, rssi, name)
            
            logger.info(msg)
            
            if is_amiigo:
                self.add_amiigo(address, name, rssi, console=console)

            sys.stdout.flush()
                
        def _device_property_changed(interface, changed, invalidated, path):
            path = unicode(path)
            logger.info('Device properties changed %s: %s at path %s', interface, changed, path)
            if interface != DEVICE_INTERFACE:
                logger.critical('should not get called with interface %s', interface)
                return
            address = (unicode(devices[path]['Address'])
                       if path in devices and 'Address' in devices[path]
                       else '<unknown>')
            if address == '<unknown>':
                address = changed.get('Address', address)
            
            properties = devices.get(path, {})
            name = unicode(properties.get('Name', ''))
            rssi = int(properties.get('RSSI', 0))
            msg = '^ Address: %s, RSSI: %s, Name: %s' % (address, rssi, name)
            logger.info(msg)

            is_amiigo = False
            if path in devices:
                is_amiigo = devices[path]['is_amiigo']
            is_amiigo = is_amiigo or self.is_amiigo(name)
            if is_amiigo and address != '<unnown>':
                self.add_amiigo(address, name, rssi, console=console)
                
            sys.stdout.flush()

        bus.add_signal_receiver(_interface_added,
                                dbus_interface='org.freedesktop.DBus.ObjectManager',
                                signal_name='InterfacesAdded')
        bus.add_signal_receiver(_device_property_changed,
                                dbus_interface='org.freedesktop.DBus.Properties',
                                signal_name='PropertiesChanged',
                                arg0=DEVICE_INTERFACE,
                                path_keyword='path')
        adapter.StartDiscovery()

        # wait a little
        self._wait_start_discovery(adapter, 3)
        
        # Scan for timeout_secs
        gobject.timeout_add(self.timeout_secs * 1000, _scan_timeout)
        try:
            self._loop.run()
        except (KeyboardInterrupt, SystemExit):
            pass
        except Exception as e:
            logger.critical('loop exception %s' % e)
        bus.remove_signal_receiver(_interface_added,
                                   dbus_interface='org.freedesktop.DBus.ObjectManager',
                                   signal_name='InterfacesAdded')
        bus.remove_signal_receiver(_device_property_changed,
                                   dbus_interface='org.freedesktop.DBus.Properties',
                                   signal_name='PropertiesChanged',
                                   arg0=DEVICE_INTERFACE,
                                   path_keyword='path')

        if self.force:
            # turn off adapter again
            self._switch_on_adapter(adapter, False)

        if console:
            print('... done listing ...')
            
        del self._manager
        del self._bus
        del self._loop
        


def main():
    description = "Amiigo Device manufacturing test (version {version})".format(version=__version__)
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("-l", "--list", dest="discover", help="List all devices.", 
                        action="store_true")
    parser.add_argument("--version", dest="version", help="Get the version of this test tool.",
                        action="store_true")
    parser.add_argument("-v", "--verbose", dest="verbosity", default=0, nargs='?', const=2,
                        help="logger verbosity")
    parser.add_argument("-a", "--amlink", dest="amlink", help="Force using amlink.", 
                        action="store_true")
    parser.add_argument("-f", "--force", dest="force", help="Force reset the adapter.", 
                        action="store_true")
    parser.add_argument("-i", "--interface", dest="interface", help="Interface adapter (default is hci0).")
    parser.add_argument("-t", "--test", dest="dut", nargs='1', help="Test a wristband given MAC address")

    if len(sys.argv) == 1:
        parser.print_help()
        
    options = parser.parse_args()

    if options.version:
        print("version {version}".format(version=__version__))

    logger.setLevel("CRITICAL")
    if options.verbosity > 0:
        logger.setLevel("DEBUG")
    dev = AmiigoDevice(force_amlink=options.amlink, 
                       force=options.force,
                       adapter=options.interface)

    if options.discover:
        dev.discover(console=True)

    if options.dut:
        dev.test(options.dut)
    
    return dev
    
if __name__ == "__main__":
    engine = main()
