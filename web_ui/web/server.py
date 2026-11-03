from bottle import route, run, static_file, template, request
import commands, time, os, psutil, json, array, zipfile, lxml
from xml2json import xml2json

intf = 'eth0'
intf_ip = commands.getoutput("ip address show dev " + intf).split()
intf_ip = intf_ip[intf_ip.index('inet') + 1].split('/')[0]

#test de que el proceso este corriendo
@route('/sys/status/<process>')
def server_static(process):
	if commands.getoutput('/sbin/pidof ' + process):
	   return '{"process":"'+process+'","status":true}'
	else:
	   return '{"process":"'+process+'","status":false}'


#apagar el server
@route('/sys/poweroff')
def server_static():
	if commands.getoutput('poweroff'):
	   return '{"status":true}'
	else:
	   return '{"status":false}'


#reiniciar el server
@route('/sys/reboot')
def server_static():
	if commands.getoutput('reboot'):
	   return '{"status":true}'
	else:
	   return '{"status":false}'



@route('/sys/input/video')
def server_static():
	if commands.getoutput('cat /etc/tvltx.input'):
	   return '{"process":"video","status":'+commands.getoutput('cat /etc/tvltx.input')+'}'
	else:
	   return '{"process":"video","status":false}'

@route('/sys/cpu/')
def server_static():
        #cpu = psutil.cpu_times()
	cpu = psutil.cpu_percent(interval=1)
	mem = psutil.virtual_memory()
	disk = psutil.disk_usage('/')
        return '{"cpu":"'+ str(cpu) +'","memory":"' + str(getattr(mem,'percent'))  + '","disk":"' + str(getattr(disk,'percent'))  + '","ip":"' + intf_ip  + '"}'


#Guarda fullseg y oneseg config
@route('/save/config/fullseg/', method='POST')
def index():
	continents =  request.forms
	result2 = {}
	result = '{'

	for (slug, title) in continents.items():
		result2.update({slug : title})
		result = result +'"'+slug+'":"'+title+'",\n'
	result = result+'}'
	try:
		with open('txltw.fullseg', 'wb') as outfile:
			json.dump(result2, outfile)
	except:
		print('No pude escribir el archivo o no lo encontre')
	commands.getoutput('python parse-fullseg.py')
	return result




#Guarda fullseg y oneseg config
@route('/save/config/muxer/', method='POST')
def index():
	from xml2json import json2xml
	continents = request.forms
	result2 = {}
	result = ''
	for (slug, title) in continents.items():
		result2.update({slug : title})
		result = slug
		print slug
	f = open("txltw.muxer.json", "wb")
	try:
		f.write(result)
	finally:
		f.close()
	commands.getoutput("python xml2json.py -t json2xml txltw.muxer.json -o /etc/tvltx.muxer.xml")

	return result


#Guarda potenica y frecuencia config
@route('/save/config/rf/<potencia>/<freq>/', method='GET')
def server_static(potencia,freq):
	result = '{"power":"'+potencia+'","frequency":"'+freq+'"}'
	f = open("tvltx.rf", "wb")
	try:
		f.write(result)
	finally:
		f.close()

	commands.getoutput('python parse-rf.py')
	return result


@route('/read/config/muxer/')
def server_static():
	commands.getoutput("python xml2json.py -t xml2json -o txltw.muxer.json /etc/tvltx.muxer.xml  --strip_text")
	result = commands.getoutput('cat txltw.muxer.json')
	return result


@route('/read/config/fullseg/')
def server_static():
    return static_file('tvltx.fullseg', root='/etc')




@route('/upload/app', method='POST')
def do_upload():
	nombre = request.forms.get('nombre')
	ncl = request.forms.get('ncl')
	upload = request.files.get('upload')
	name, ext = os.path.splitext(upload.filename)
	if ext not in ('.zip', '.tar.gz'):
		return "File extension not allowed."
	save_path = "/root/run_path/apps/{nombre}".format(nombre=nombre)
	if not os.path.exists(save_path):
		os.makedirs(save_path)
	file_name = "{file}".format(file=upload.filename)
	file_path = "{path}/{file}".format(path=save_path, file=upload.filename)
	upload.save(file_path)
	zfile = zipfile.ZipFile(file_path)
	zfile.extractall(save_path)
	import configuracion
	configuracion.cambiar_app(nombre,ncl)
	return "File successfully saved to '{0}'.".format(save_path)


@route('/apps/list')
def server_static():
	for file in os.listdir("/root/run_path/apps"):
		resultado = '[';
    	if file.endswith(".txt"):
	        resultado = resultado + file + ','
	resultado = resultado + ']'
	return  resultado

@route('/sys/run/<process>')
def server_static(process):
	if process ==  'run_system-new':
		if commands.getoutput('/sbin/pidof ' + process + '.sh'):
			return '{"process":"'+process+'","status":true,"active":"already working"}'
		else:
			if os.popen(process + '.sh'):
		   		return '{"process":"'+process+'","status":true}'
			else:
		   		return '{"process":"'+process+'","status":false}'	
	else:
		if os.popen(process + '.sh'):
	   		return '{"process":"'+process+'","status":true}'
		else:
	   		return '{"process":"'+process+'","status":false}'



#carga los archivos en la web
@route('/<filepath:path>')
def server_static(filepath):
    return static_file(filepath, root='/root/web')



run(host='0.0.0.0', port=8080, debug=True)
