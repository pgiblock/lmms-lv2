#!/usr/bin/env python
import os
import shutil
from waflib import Logs

# Variables for 'waf dist'
APPNAME = 'lmms.lv2'
VERSION = '0.1.0'

LV2DIR  = '/usr/local/lib/lv2'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

    opt.add_option('--use-urid', dest='use_urid', default=False, action='store_true',
                   help='Use the urid extension in instead of uri-map')
    opt.add_option('--use-atom', dest='use_atom', default=False, action='store_true',
                   help='Use the atom extension in instead of event')

def configure(conf):
    conf.load('compiler_c')
    conf.env.append_value('CFLAGS', '-Wall')
    conf.env.append_value('CFLAGS', '-O3')
    conf.env.append_value('CFLAGS', '-std=c99')
    conf.env.append_value('CFLAGS', '-fPIC')

    # TODO: don't hardcode
    conf.env.LV2DIR = LV2DIR;

    # Required
    conf.check_cfg(package='lv2core', atleast_version='6.0',
            args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-state',
            uselib_store='LV2_STATE', args=['--cflags', '--libs'])

    # Equivalents
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-urid',
            uselib_store='LV2_URID', args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-uri-map',
            uselib_store='LV2_URI_MAP', args=['--cflags', '--libs'])

    # Equivalents
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-atom',
            uselib_store='LV2_ATOM', args=['--cflags', '--libs'])
    conf.check_cfg(package='lv2-lv2plug.in-ns-ext-event',
            uselib_store='LV2_EVENT', args=['--cflags', '--libs'])

    # Optional
    conf.check_cfg(package='gtk+-2.0', atleast_version='2.18.0',
            uselib_store='GTK2', args=['--cflags', '--libs'], 
            mandatory=False)

    use_urid = False
    if not conf.env.HAVE_LV2_URI_MAP and not conf.env.HAVE_LV2_URID:
        # Got nothing
        conf.fatal('''You must have either the uri-map (recommended) or urid extension''')
    elif conf.options.use_urid:
        # User actually wants urid
        if conf.env.HAVE_LV2_URID:
            use_urid = True
        else:
            conf.fatal('''You asked to use the urid extension, but it wasn't found''')
    else:
        # Prefer uri-map over urid
        use_urid = not conf.env.HAVE_LV2_URI_MAP
    if use_urid:
        conf.msg('Warning',
                 '''LV2 urid extension support is enabled!! This breaks '''
                 '''compatibility and is for development purposes only!!''', 'BOLD')
        conf.define('USE_LV2_URID', 1)

    use_atom = False
    if not conf.env.HAVE_LV2_EVENT and not conf.env.HAVE_LV2_ATOM:
        # Got nothing
        conf.fatal('''You must have either the event (recommended) or atom extension''')
    elif conf.options.use_atom:
        # User actually wants atom
        if conf.env.HAVE_LV2_ATOM:
            use_atom = True
        else:
            conf.fatal('''You asked to use the atom extension, but it wasn't found''')
    else:
        # Prefer event over atom
        use_atom = not conf.env.HAVE_LV2_ATOM
    if use_atom:
        conf.msg('Warning',
                 '''LV2 atom extension support is enabled!! This breaks '''
                 '''compatibility and is for development purposes only!!''',
                 'BOLD')
        conf.define('USE_LV2_ATOM', 1)

    # Pre-calculate pattern for plugin *.so files
    pat = conf.env['cshlib_PATTERN']
    if pat.startswith('lib'):
        pat = pat[3:]
    conf.env['pluginlib_PATTERN'] = pat

    conf.write_config_header('src/config.h')


def build(bld):
    bld.recurse('src')

# vim: ts=8:sts=4:sw=4:et
