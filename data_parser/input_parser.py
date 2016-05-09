""" Input data parser
This should be just simple parsing of known Amiigo offline data files.
This is not supposed to be called by engines directly, only by instrumented instances that work offline.

The result is a data structure that exactly mimicks the input to engines when called by platform.
  This ensures the engine code will *only* need to care about the format as supplied by platform/server.

Copyright Amiigo Inc.
"""

import sys
import os
import time
import re
import zipfile
import StringIO
import numpy as np
import json
import datetime
from dateutil.parser import parse as date_parser
from os.path import basename
import csv


class ParseError(ValueError):
    pass


class ParseErrorFile(ParseError):
    pass


class ParseErrorNotExist(ParseErrorFile):
    pass


class ParseErrorUnknownType(ParseErrorFile):
    pass


# ----------------------------------------------------------------------------------------------------------------------
class SensorData(object):
    """Sensor data
    """
    def __init__(self, name, data, device='amiigo', **kwargs):
        """
        Constructor
        Inputs:
            name    - name of the sensor data type
            data    - actual sensor data
            device  - device type
            kwargs  - additional arguments:
        """
        self.name = name

        if name == 'accelerometer':
            if isinstance(data, basestring):
                data = json.loads(data)
            self.x = np.int16(data[0])
            self.y = np.int16(data[1])
            self.z = np.int16(data[2])
            self.data_repr = (lambda self: '[%s,%s,%s]' % (self.x, self.y, self.z))

        elif name == 'temperature':
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = json.loads(data)
            try:
                self.celsius = data['celsius']
            except:
                self.celsius = float(data)
            self.data_repr = (lambda self: '[%s]' % self.celsius)
            
        elif name == 'timestamp' or name == 'debug':
            self.name = 'timestamp'
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = data.replace('True', '1')
                data = data.replace('False', '0')
                data = json.loads(data)
            if isinstance(data, float) or isinstance(data, int) or isinstance(data, np.double):
                data = {'seconds': data, 'errcode': data, 'ticks': np.uint32(data * 128)}
            elif isinstance(data, list):
                data = {'seconds': data[0] / 128.0, 'flags': np.uint8(data[1]), 'errcode': data[0], 'ticks': data[0]}

            self.flags = np.uint8(data.get('flags', 0))
            self.seconds = data.get('seconds', 0)
            self.reboot = data.get('reboot', self.flags & 0x80)
            self.debug = data.get('debug', self.flags & 0x10)
            self.fast_rate = data.get('fast_rate', self.flags & 0x01)
            self.sleep = data.get('sleep', self.flags & 0x02)
            self.passive = data.get('passive', False)
            self.errcode = data.get('errcode', 0)
            self.data = data
            self.ticks = data.get('ticks', 0)

            self.data_repr = (lambda self: '[%s, %s]' % (self.ticks, self.flags))
        elif name == 'lightsensor':
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = json.loads(data)
            if isinstance(data, list):
                data = dict(data)
            self.ir = int(data.get('ir', 0))
            self.red = int(data.get('red', 0))
            self.off = int(data.get('off', 0))
            self.data_repr = (lambda self: '[%s,%s,%s]' % (self.ir, self.off, self.red))
        elif name == 'lightsensor_config':
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = data.replace('(', '[')
                data = data.replace(')', ']')
                data = json.loads(data)
            if isinstance(data, list):
                data = dict(data)
            self.dac_on = int(data['dac_on'])
            self.gain = int(data['gain'])
            self.level_led = int(data['level_led'])
            self.log_size = int(data['log_size'])
            self.flags = int(data.get('flags', data.get('reserved', 0)))
            self.manual = data.get('manual', self.flags & 1)
            self.worn = data.get('worn', self.flags & 2)
            self.data_repr = (lambda self: '[%s, %s, %s, %s]' % (self.dac_on, self.gain, self.level_led, self.log_size))
        elif name == 'log_count':
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = data.replace("(", '[')
                data = data.replace(")", ']')
                data = json.loads(data)
            if isinstance(data, list):
                data = dict(data)
            self.log_timestamp = np.uint32(data['log_timestamp'])
            self.log_accel_count = np.uint16(data['log_accel_count'])
            self.old_timestamp = np.uint32(data['old_timestamp'])
            self.timestamp = np.uint32(data['timestamp'])
            self.data_repr = (lambda self: '[%s, %s, %s, %s]' % (self.log_timestamp, self.log_accel_count, self.old_timestamp, self.timestamp))
        elif name == 'event':
            if isinstance(data, basestring):
                data = data.replace("'", '"')
                data = data.replace("(", '[')
                data = data.replace(")", ']')
                data = json.loads(data)
            if isinstance(data, list):
                data = dict(data)
            self.flags = np.uint8(data['flags'])
            self.data_repr = (lambda self: '[%s]' % self.flags)
        else:
            self.data = data
            self.data_repr = (lambda self: '[%s]' % self.data)

    def __repr__(self):
        return '%s,%s' % (self.name, self.data_repr(self))


class AmiigoParser(object):
    """ Generic parser for Amiigo data files
    """
    
    def __init__(self, **kwargs):
        """ Constructor
        """
        # Match for (LABEL)[_REF][_(COMMENT)].csv
        # label is alphanumeric, without underscore
        # comment can be anything
        self.match_string = r"^(?P<label>(?:[^\W_]|-)+)_REF(?:_\w*)?"

        self.ctime = np.datetime64(date_parser('1981-06-12T04:25:21.565Z'))
        # default options
        self._default_options = {'start_timestamp': str(self.ctime),
                                 'end_timestamp': str(self.ctime + np.timedelta64(30, 's')),
                                 'requires_shoepod': False}

        self.verbosity = kwargs.get('verbosity', 0)
        self.device = 'amiigo'
        self.fname = ''
        self.name = ''

    def get_error_code(self, flags, errcode):
        """ Decode firmware error codes
        """
        errline = (errcode & 0x00000FFF)
        errclass = (errcode & 0x0000F000) >> 12
        errcode = (errcode & 0xFFFF0000) >> 16
        if errcode:
            errcode = [(errcode & 0xFF00) >> 8, errcode & 0xFF]
            errcode = chr(errcode[0]) + chr(errcode[1])
        
        return errclass, errcode, errline

    def get_default_options(self):
        """get best default options at hand
        """
        options = self._default_options
        options.update({
            'start_timestamp': str(self.ctime),
            'end_timestamp': str(self.ctime + np.timedelta64(30, 's')),
        })

        return options

    def convert_data(self, x, device_type='amiigo-wristband', header=[]):
        """ Convert old raw array to latest engine format
        Inputs:
            x           - list of accelerometer values
            device_type - device type to use
        """  

        def _convert_sensors():
            if len(x[0]) == 3:
                return [SensorData('accelerometer', np.double(d[:3])) for d in x]
            _sensors = []
            return _sensors

        options = self.get_default_options()
        return {
            'Boz': {
                'sensors': _convert_sensors(),
                'type': device_type}
        }, options
    
    def parse_csv(self, fname, content=None):
        """ Read csv file
        Inputs:
          fname - full file path
          content - file content (optional)
        Outputs:
          x - matrix of all data
        """

        if content is None:
            with open(fname, 'rb') as f:
                reader = csv.reader(f)
                x = list(reader)
        else:
            buf = StringIO.StringIO(content)
            reader = csv.reader(buf)
            x = list(reader)

        header = []
        if x:
            if isinstance(x[0][0], basestring):
                header = x[0]
                x = x[1:]
        device_type = 'amiigo-wristband'
        _data, options = self.convert_data(x, device_type=device_type, header=header)
        return [(_data, options, None)], ''
    
    def parse_json(self, fname, label, content=None):
        """ Read json file 
        Read all file versions and convert to the latest format.
        
        Inputs:
          fname - full file path
          label - label based on file name
          content - file content (optional)
        """

        if content is None:        
            # Open the file and read content
            with open(fname) as f:
                content = f.readlines()
        content = "".join(content)
        input_data = json.loads(content)
        
        def read_json_set(data):
            """ read single json item
            """
            _data = data['data']
            options = data['options']
            pk = data.get('pk')
            for device_name in _data.keys():
                sensors = _data[device_name]['sensors']

                new_data = []
                if isinstance(sensors, list):
                    # Flattened data format
                    new_data = [SensorData(d[0], d[1]) for d in sensors]
                elif 'accelerometer' in sensors:
                    # Non-flattened format
                    new_data = [SensorData('accelerometer', d) for d in sensors['accelerometer']]
                _data[device_name]['sensors'] = new_data
            
            return [(_data, options, pk)]
        
        data_lst = []
        if isinstance(input_data, list):
            data = input_data[0]
            if 'data_sets' in data:
                # let it override label
                try:
                    label = data.get('name', label).decode('ascii')
                except:
                    pass
                for _data in data['data_sets']:
                    data_lst += read_json_set(_data)
            else:
                data_lst = read_json_set(data)
            return data_lst, label 
        
        _data = input_data

        options = {}
        if 'accelerometer' in input_data:
            x = input_data['accelerometer']
            _data, options = self.convert_data(x)
        elif 'accel' in input_data:
            x = input_data['accel']
            _data, options = self.convert_data(x)
        elif 'devices' in input_data:
            x = input_data['devices']
            if '0' in x:
                x = input_data['devices']['0']
                _data, options = self.convert_data(x)
            elif '1' in x:
                x = input_data['devices']['1']
                _data, options = self.convert_data(x, 'amiigo-shoepod')
                
        return [(_data, options, None)], label
        
    def parse_log(self, fname, content=None):
        """ Read amlink log
        Inputs:
            fname - full file path
            content - file content (optional)
        """
        # read multiple files
        _data = []
        if content is None:        
            # Open the file and read content
            with open(fname) as f:
                content = f.readlines()
        
        seconds = 0  # very rough estimate of duration
        fs_accel = 4
        for sensor in content:
            d = json.loads(sensor)
            if d[0] != 'accelerometer' and d[0] != 'timestamp' and d[0] != 'temperature':
                elem = [e for idx, e in enumerate(d) if idx > 0]
            else:
                elem = d[1]
            elem = SensorData(d[0], elem)
            _data.append(elem)
            
            if elem.name == 'accelerometer':
                seconds = seconds + 1.0 / fs_accel
            elif elem.name == 'timestamp' and not elem.debug:
                if elem.passive:
                    fs_accel = 4 
                elif elem.sleep:
                    fs_accel = 1
                elif elem.fast_rate:
                    fs_accel = 20

        _data = {'Boz': {'sensors': _data, 'type': 'amiigo-wristband'}}
        options = self.get_default_options()
        if seconds > 0:
            options['end_timestamp'] = str(date_parser(options['start_timestamp']) + datetime.timedelta(seconds=seconds))        
        return [(_data, options, None)], ''

    def parse_lst(self, fname, content=None):
        """ Read a list of files
        Inputs:
            fname - full file path to file containing the list
            content - file content (optional)
        """
        
        data_lst = []

        if content is None:        
            # Open the file and read content
            with open(fname) as f:
                content = f.readlines()
        
        for fname in content:
            data, _, _ = self.parse(fname.strip())
            data_lst += data
        
        return data_lst, ''

    def parse_zip(self, fname, label, content=None):
        """ Read a bunch of files from a zip file
        Inputs:
            fname - zip file name path
            content - file content (optional)
        """
        
        data_lst = []
        if content is None:
            zf = zipfile.ZipFile(fname)
        else:
            buf = StringIO.StringIO(content)
            zf = zipfile.ZipFile(buf, 'r')
        for fname in zf.namelist():
            if fname.endswith('/'):
                # ignore inside directories
                continue
            self.ctime = np.datetime64(datetime.datetime(* zf.getinfo(fname).date_time))
            intcont = zf.read(fname)
            try:
                data, _, label = self.parse(fname, content=intcont)
                data_lst += data
            except (KeyboardInterrupt, SystemExit):
                raise
            except:
                _fname = basename(fname)
                if self.verbosity and not _fname.startswith('.'):
                    # ignore normal exceptions and proceed to next file
                    print 'Ignoring %s' % fname
    
        return data_lst, label
        
    def parse(self, fname, content=None):
        """ Read a single file, filter, and parse as one data matrix
        Inputs:
            fname   - full file path to file for parsing
            content - file content (optional)
        """

        if content is None:
            self.ctime = np.datetime64(date_parser(time.ctime(os.stat(fname).st_ctime)))
        _fname = basename(fname)
        
        m = re.match(self.match_string, _fname, re.IGNORECASE)
        label = _fname
        if m is not None:
            label = m.group('label').lower()
        
        data_lst = []
        _name, extension = os.path.splitext(_fname.lower())
        
        try:
            if extension == '.json':
                data, label = self.parse_json(fname, label, content=content)
            elif extension == '.log':
                data, _ = self.parse_log(fname, content=content)
            elif extension == '.csv':
                data, _ = self.parse_csv(fname, content=content)
            elif extension == '.lst':
                data, _ = self.parse_lst(fname, content=content)
            elif extension == '.zip':
                data, label = self.parse_zip(fname, label, content=content)
            else:
                if content is None and not os.path.exists(fname):
                    raise ParseErrorNotExist('File "%s" not found' % _fname)
                raise ParseErrorUnknownType('Unknown file type (%s)' % _fname)
            # aggregate all
            data_lst += data
        except (KeyboardInterrupt, SystemExit):
            raise
        except ParseErrorUnknownType:
            # don't bug about unknown file types
            pass
        except:
            if not os.path.isfile(fname):
                # Re-raise the exception if no such file
                exec_info = sys.exc_info()
                raise exec_info[0], exec_info[1], exec_info[2]
            if self.verbosity and not _fname.startswith('.'):
                # ignore normal exceptions and proceed to next file
                print 'Ignoring %s' % _fname
            
        # Keep the name
        self.fname = _name
        self.name = _name
        
        return data_lst, label, fname
