var muxer_data;
$(document).ready(function() {
	$.getJSON('../txltw.fullseg', function(response) {
		$('#fullseg ')
		var frm = document.getElementById("fullseg");
	    var i;	 
	    for (i in response) {
	        if (i in frm.elements) {
	            frm.elements[i].value = response[i];
	        }
	    }
	});
	
	$.getJSON('../read/config/muxer/', function(response) {
		$('#muxer')
		var frm = document.getElementById("muxer");
	    var i;	 
	    for (i in response.tmm.body.output) {
	        if (i in frm.elements) {
	            frm.elements[i].value = response.tmm.body.output[i];
	        }
	    }
		muxer_data = response;
		console.log(muxer_data);
	});
	$.getJSON('../tvltx.rf', function(response) {
		$('#muxer')
		var frm = document.getElementById("muxer");
	    frm.elements['potencia'].value = response.power;
	    console.log(response);	
	});
});

$("#fullseg").submit(function() {
    $.ajax({
        type: "POST",
        url: '../save/config/fullseg/',
        data: $("#fullseg").serialize(), // serializes the form's elements.
        success: function(data)
        {
            alert("Configuración guardada, debe reiniciar la transmisión para que sea aplicada."); // show response from the php script.
            console.log(data);
        }
    });
    return false;
});


$("#muxer").submit(function() {
	$('#muxer')
	var frm = document.getElementById("muxer");
	var i;	 
	for (i in frm.elements) {
        if (frm.elements[i].name in muxer_data.tmm.body.output) {
	        muxer_data.tmm.body.output[frm.elements[i].name] = frm.elements[i].value;
	        muxer_data.tmm.body.inputs.pmt[0]['@name'] = frm.elements[i].value;
	        muxer_data.tmm.body.inputs.pmt[1]['@name'] = frm.elements[i].value;
	    }
    }
    potencia = $("#potencia").val();
	freq = muxer_data.tmm.body.output['@broadcastfrequency'];
	$.getJSON('../save/config/rf/'+potencia+'/'+freq+'/', function(response) {
		console.log(response)
	});
    console.log(JSON.stringify(muxer_data))
    $.ajax({
        type: "POST",
        url: '../save/config/muxer/',
        data: JSON.stringify(muxer_data), // serializes the form's elements.
        success: function(data)
        {     
            alert("Configuración guardada, reiniciando la transmisión."); // show response from the php script.
			$.ajax({
			  	url: "../sys/run/stop_system"
			});
            $.ajax({
			  	url: "../sys/run/stop_system"
			})
			  .done(function(result) {
			  	console.log("System stop");
			  	$.ajax({
			  		url: "../sys/run/run_system"
				})
			  	  .done(function(result) {
			  	  	console.log("System run");
			  	});
			});	
			
        }
    });
	
    return false;
});



$("#guardar-reiniciar").click(function() {
    $.ajax({
        type: "POST",
        url: '../save/config/fullseg/',
        data: $("#fullseg").serialize(), // serializes the form's elements.
        success: function(data)
        {
            alert("Configuración guardada, reiniciando la transmisión."); // show response from the php script.
            $.ajax({
			  	url: "../sys/run/stop_system"
			})
			  .done(function(result) {
			  	console.log("System stop");
			  	$.ajax({
			  		url: "../sys/run/run_system"
				})
			  	  .done(function(result) {
			  	  	console.log("System run");
			  	});
			});	
        }
    });
    return false;
});

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

