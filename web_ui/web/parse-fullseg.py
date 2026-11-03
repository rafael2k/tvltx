import json


with open('txltw.fullseg') as f:
	json_data = json.load(f)
	def foo():
		string = ("is_hd_service=\""+json_data['is_hd_service']+"\"\n"
			"video_pid=\""+hex(int(json_data['video_pid']))+"\"\n"
			"pcr_pid=\""+hex(int(json_data['pcr_pid']))+"\"\n"
			"video_bitrate=\""+json_data['video_bitrate']+"\"\n"
			"ts_package_size=\""+json_data['ts_package_size']+"\"\n"
			"card_number=\""+json_data['card_number']+"\"\n"
			"video_input_type=\""+json_data['video_input_type']+"\"\n"
			"video_input_format=\""+json_data['video_input_format']+"\"\n"
			"video_output_format=\""+json_data['video_output_format']+"\"\n"
			"video_entropy_coding=\""+json_data['video_entropy_coding']+"\"\n"
			"video_gop_size=\""+json_data['video_gop_size']+"\"\n"
			"video_noise_reduction=\""+json_data['video_noise_reduction']+"\"\n"
			"number_audio=\""+json_data['number_audio']+"\"\n"
			"audio_1_pid=\""+hex(int(json_data['audio_1_pid']))+"\"\n"
			"audio_2_pid=\""+hex(int(json_data['audio_2_pid']))+"\"\n"
			"audio_3_pid=\""+hex(int(json_data['audio_3_pid']))+"\"\n"
			"audio_4_pid=\""+hex(int(json_data['audio_4_pid']))+"\"\n"
			"audio_1_input_mask=\""+hex(int(json_data['audio_1_input_mask']))+"\"\n"
			"audio_2_input_mask=\""+hex(int(json_data['audio_2_input_mask']))+"\"\n"
			"audio_3_input_mask=\""+hex(int(json_data['audio_3_input_mask']))+"\"\n"
			"audio_4_input_mask=\""+hex(int(json_data['audio_4_input_mask']))+"\"\n"
			"audio_1_bitrate=\""+json_data['audio_1_bitrate']+"\"\n"
			"audio_2_bitrate=\""+json_data['audio_2_bitrate']+"\"\n"
			"audio_3_bitrate=\""+json_data['audio_3_bitrate']+"\"\n"
			"audio_4_bitrate=\""+json_data['audio_4_bitrate']+"\"\n"
			"av_offset=\""+json_data['av_offset']+"\"")
		return string
	f = open("/etc/tvltx.fullseg", "w")
	try:
		f.write(foo())
	finally:
		f.close()


with open('txltw.fullseg') as f:
	json_data = json.load(f)
	def foo():
		string = ("is_hd_service=\"False\"\n"
			"video_pid=\""+hex(int(json_data['oneseg_video_pid']))+"\"\n"
			"pcr_pid=\""+hex(int(json_data['oneseg_pcr_pid']))+"\"\n"
			"video_bitrate=\""+json_data['oneseg_video_bitrate']+"\"\n"
			"ts_package_size=\""+json_data['oneseg_ts_package_size']+"\"\n"
			"card_number=\""+json_data['oneseg_card_number']+"\"\n"
			"video_input_type=\""+json_data['oneseg_video_input_type']+"\"\n"
			"video_input_format=\""+json_data['video_input_format']+"\"\n"
			"video_output_format=\""+json_data['oneseg_video_output_format']+"\"\n"
			"video_entropy_coding=\""+json_data['oneseg_video_entropy_coding']+"\"\n"
			"video_gop_size=\""+json_data['oneseg_video_gop_size']+"\"\n"
			"video_noise_reduction=\""+json_data['oneseg_video_noise_reduction']+"\"\n"
			"number_audio=\""+json_data['oneseg_number_audio']+"\"\n"
			"audio_1_pid=\""+hex(int(json_data['oneseg_audio_1_pid']))+"\"\n"
			"audio_2_pid=\""+hex(int(json_data['oneseg_audio_2_pid']))+"\"\n"
			"audio_3_pid=\""+hex(int(json_data['oneseg_audio_3_pid']))+"\"\n"
			"audio_4_pid=\""+hex(int(json_data['oneseg_audio_4_pid']))+"\"\n"
			"audio_1_input_mask=\""+hex(int(json_data['oneseg_audio_1_input_mask']))+"\"\n"
			"audio_2_input_mask=\""+hex(int(json_data['oneseg_audio_2_input_mask']))+"\"\n"
			"audio_3_input_mask=\""+hex(int(json_data['oneseg_audio_3_input_mask']))+"\"\n"
			"audio_4_input_mask=\""+hex(int(json_data['oneseg_audio_4_input_mask']))+"\"\n"
			"audio_1_bitrate=\""+json_data['oneseg_audio_1_bitrate']+"\"\n"
			"audio_2_bitrate=\""+json_data['oneseg_audio_2_bitrate']+"\"\n"
			"audio_3_bitrate=\""+json_data['oneseg_audio_3_bitrate']+"\"\n"
			"audio_4_bitrate=\""+json_data['oneseg_audio_4_bitrate']+"\"\n"
			"av_offset=\""+json_data['oneseg_av_offset']+"\"")
		return string
	f = open("/etc/tvltx.oneseg", "w")
	try:
		f.write(foo())
	finally:
		f.close()
