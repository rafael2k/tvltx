


$(document).ready(function() {

	$("#video_input_format").change(function(event) {
		var option = $(this).find('option:selected');
		
		//fullseg output - clear and display options
		var fullseg_hides = $("#video_output_format").find("[data-multiplo='" + option.data('multiplo') + "']");
		var options = $("#video_output_format").find('option');
		options.addClass('hidden');
		fullseg_hides.removeClass('hidden');
		var selected = $("#video_output_format").find("option:selected");	
		if (selected.data('multiplo') !== option.data('multiplo')) {
			selected.attr({"selected":""});
			$("#video_output_format").val('');
		};

		//one output - clear and display options
		var oneseg_hides = $("#video_output_format_oneseg").find("[data-multiplo='" + option.data('multiplo') + "']");
		var options = $("#video_output_format_oneseg").find('option');
		options.addClass('hidden');
		oneseg_hides.removeClass('hidden');
		var selected = $("#video_output_format_oneseg").find("option:selected");	
		if (selected.data('multiplo') !== option.data('multiplo')) {
			selected.attr({"selected":""});
			$("#video_output_format_oneseg").val('');
		};

	});

	$(':checkbox').checkboxpicker(),
	estados = setInterval(function(){ checkstatus() }, 5000);
	
	var ocupado = false;
	$('.estado-bts_tx').click(function() {
		if ( $('.estado-bts_tx').attr('estado') === 'false' ){
				$.ajax({
			  		url: "../sys/run/run_system"
				})
			  	  .done(function(result) {
			  	  	console.log("System run");
			  	});
		}
		else {
			$.ajax({
			  	url: "../sys/run/stop_system"
			})
			  .done(function(result) {
			  	console.log("System stop");
			});				
		}
	});


});


function get_status(process){
	switch(process){
		case "video":
			var getUrl = "../sys/input/"+process;
			break;
		default:
			var getUrl = "../sys/status/"+process;
			break;
	}
	$.ajax({
  		url: getUrl
	})
  	  .done(function(result) {
  	  	result = JSON.parse(result);
  	  	if(result.process != 'bts_tx'){
  	  		$("#estado-"+result.process).prop('checked', result.status);
  	  	}
  	  	else {
  	  		
  	  		if(result.status === false){
  	  			console.log(result);
  	  			$(".estado-"+result.process).html('Encender');	
  	  			$(".estado-"+result.process).attr({'estado':false});	
  	  		}
  	  		else {
  	  			$(".estado-"+result.process).html('Apagar');
  	  			$(".estado-"+result.process).attr({'estado':true});	
  	  		}
  	  	}
  	  	
  	});
}

function get_system(){
	$.ajax({
  		url: '../sys/cpu/'
	})
  	  .done(function(result) {
  	  	result = JSON.parse(result);
  	  	$("#status-cpu").html(result.cpu  + '%');
  	  	$("#status-memory").html(result.memory + '%');
  	  	$("#status-disk").html(result.disk + '%');
  	  	$("#status-ip").html(result.ip);
  	});
}

function statusColor(valor){

}

function checkstatus() {
	ocupado = true;
	get_status('bts_tx'),
	get_status('x264'),
	get_status('video');
	get_system();
	ocupado = false;
}



