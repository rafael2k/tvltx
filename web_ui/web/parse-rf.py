import json

with open('tvltx.rf') as f:
	json_data = json.load(f)
	def foo():
		string = ("frequency="+json_data['frequency']+".14\n"
			"power="+json_data['power']+"\n"
			"mode=3"+"\n"
			"guard_interval=2"+"\n"
			"layer1_mod_type=1"+"\n"
			"layer1_coderate=1"+"\n"
			"layer1_time_interleave=3"+"\n"
			"layer2_mod_type=2"+"\n"
			"layer2_coderate=2"+"\n"
			"layer2_time_interleave=2"+"\n"
			"0x0   B"+"\n"
			"0x10  A"+"\n"
			"0x11  A"+"\n"
			"0x12  B"+"\n"
			"0x14  A"+"\n"
			"0x24  A"+"\n"
			"0x27  A"+"\n"
			"0x100 B"+"\n"
			"0x101 B"+"\n"
			"0x111 B"+"\n"
			"0x112 B"+"\n"
			"0x113 B"+"\n"
			"0x116 B"+"\n"
			"0x1fc8 A"+"\n"
			"0x200 A"+"\n"
			"0x211 A"+"\n"
			"0x212 A"+"\n"
			"0x216 A")
		return string
	f = open("/etc/tvltx.rf", "wb")
	try:
		f.write(foo())
	finally:
		f.close()