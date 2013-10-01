#!/usr/bin/env python

import errno, os, os.path
import sys
from pprint import pprint
from rdflib import Graph
from rdflib import Literal, URIRef, BNode, Namespace
from rdflib import RDF, RDFS
from xml.etree import ElementTree

def bind(prefix, uri):
    g.bind(prefix, uri)
    return Namespace(uri)

def get_port(s):
	symbol = g.value(subject=s, predicate=LV2.symbol)
	index  = g.value(subject=s, predicate=LV2.term('index'))
	return index, symbol

g = Graph()

LV2    = bind('lv2',    'http://lv2plug.in/ns/lv2core#')
PSET   = bind('pset',   'http://lv2plug.in/ns/ext/presets#')
PGPLUG = bind('pgplug', 'http://pgiblock.net/plugins/')
PGNS   = bind('pgns',   'http://pgiblock.net/ns/')
g.parse(sys.argv[1], format='turtle')

ports = []
for s in g.subjects(predicate=RDF.type, object=LV2.InputPort):
	ports.append(get_port(s))
for s in g.subjects(predicate=RDF.type, object=LV2.OutputPort):
	ports.append(get_port(s))

ports.sort()
with open(sys.argv[2], 'w') as f:
	header_guard = '%s__' % os.path.split(sys.argv[2])[1].replace('.','_').upper()
	f.write('#ifndef %s\n' % header_guard)
	f.write('#define %s\n' % header_guard)
	f.write('// Automatically generated file: do not edit.\n\n');
	f.write('enum {\n');
	for index, symbol in ports:
		f.write('\tPORT_%s\t = %s,\n' % (symbol.upper(), index))
	f.write('};\n')
	f.write('#endif\n')
