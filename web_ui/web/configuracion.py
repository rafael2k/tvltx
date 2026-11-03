from lxml import etree
from lxml import objectify
import io

def cambiar_app(nombre,archivo):
	doc = etree.parse('/etc/config-test.tmm')
	ait = doc.xpath("/tmm/body/inputs/ait/carouselref")
	ait[0].attrib['appname'] = nombre
	ait[0].attrib['entrypoint'] = archivo
	f = open("/etc/tvltx.muxer.xml", "w")
	try:
		f.write('<?xml version="1.0" encoding="ISO-8859-1"?>'+etree.tostring(doc, pretty_print=True))
	finally:
		f.close()
	return ait[0].attrib['appname']