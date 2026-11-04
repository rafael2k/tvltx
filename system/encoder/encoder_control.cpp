/*
    TVLTx - TV Lovers Encoder
    Copyright (C) 2015-2017 SDR Telecom

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include "project/XMLProject.h"

// temporary hack to cope with these defines also present in tmm source code
#undef CARD_TYPE_TEST
#undef CARD_TYPE_DEKTEC
#undef CARD_TYPE_BLACKMAGIC
#undef CARD_TYPE_DVEO
#undef CARD_TYPE_V4L2

#undef INPUT_TYPE_NONE
#undef INPUT_TYPE_SDI
#undef INPUT_TYPE_HDMI
#undef INPUT_TYPE_COMPOSITE
#undef INPUT_TYPE_COMPONENT
#undef INPUT_TYPE_OPTICALSDI
#undef INPUT_TYPE_SVIDEO


#include "encoder_opts.h"
#include "macros.h"
#include "input.h"
#include "encoder.h"
#include "output.h"

// global var with all the services configuration
service_config services[MAX_SERVICES];
input_config inputs[MAX_INPUT_INTERFACES];

int nr_services;
int nr_inputs;

bool running;
static void sigfunc(int signum);

int start_encoders(Project *project)
{
    memset (services, 0, MAX_SERVICES * sizeof(service_config));
    memset (inputs, 0, MAX_INPUT_INTERFACES * sizeof(input_config));

    tvltx_info("TVLTx Encoder Version 0.2.\n");
    tvltx_info("Copyright: SDR Telecom 2015-2018.\n");
    tvltx_info("License: GNU AFFERO Version 3 plus non-commercial use restriction.\n");

    nr_inputs = 0;
    map<int, InputCardInfo*>::iterator ici;
    ici = project->getInputCardList()->begin();
    while (ici != project->getInputCardList()->end()) {
        int id = ici->second->getId();
        if (id >= MAX_INPUT_INTERFACES){
            tvltx_info("Error: project input id must be incremental, starting from 0 and smaller then %d\n", MAX_INPUT_INTERFACES);
            return EXIT_FAILURE;
        }
        tvltx_info("we have input cards..\n");
        inputs[id].input_id = id;
        inputs[id].card_number = ici->second->getCardNumber();
        inputs[id].input_type = ici->second->getType();
        inputs[id].card_type = ici->second->getBrand();
        inputs[id].audio_input_channel_nr = ici->second->getNumberOfChannels();
        inputs[id].video_input_mode = ici->second->getMode();
        snprintf(inputs[id].video_device, sizeof(inputs[id].video_device), "%s",
                 ici->second->getVideoDevice().c_str());
        snprintf(inputs[id].audio_device, sizeof(inputs[id].audio_device), "%s",
                 ici->second->getAudioDevice().c_str());
        snprintf(inputs[id].video_std, sizeof(inputs[id].video_std), "%s",
                 ici->second->getVideoStandard().c_str());
        nr_inputs++;
        ++ici;
    }

    int srv_nr;
    unsigned char stream_type;
    ProjectInfo* proj;
    int condRet;
    vector<pmtViewInfo*>* newTimeline;
    vector<pmtViewInfo*>::iterator itPmt;
    map<unsigned short, vector<ProjectInfo*>*>::iterator it;
    vector<ProjectInfo*>::iterator itPrj;

    proj = Project::getFirstProjectReversed(project->getProjectList(), PT_TIMELINE);
    if (proj) {
        newTimeline = ((Timeline*)proj)->currTimeline(300000000, &condRet);
        if (!newTimeline) {
            tvltx_info("Unexpected error: No timeline.\n");
            return -1;
        }
        itPmt = newTimeline->begin();
        //Iterando sobre todos os serviços da linha do tempo

        srv_nr = 0;
        while (itPmt != newTimeline->end()) {
            if ((*itPmt)->pv->getServiceType() == SRV_TYPE_ONESEG) {
                services[srv_nr].is_oneseg = true;
                // (*itPmt)->pv->getPid(); //exemplo pra pegar o pid desta PMT
            }
            //isso mostra como pegar todos os fluxos associados a este serviço
            it = (*itPmt)->pv->getProjectInfoList()->begin();
            while(it != (*itPmt)->pv->getProjectInfoList()->end()) {
                if (it->second) {
                    itPrj = it->second->begin();
                    while(itPrj != it->second->end()) {
                        if ((*itPrj) && ((*itPrj)->getProjectType() == PT_INPUTDATA)) {
                            //essa parte você já conhece
                             InputData *inputData = ((InputData*)(*itPrj));

                            services[srv_nr].input_id = inputData->getRefInputCardId();

                            stream_type = inputData->getStreamType();
                            // tvltx_info ("srv_nr: %dwe got a service type %d\n", stream_type);
                            if (stream_type == 27){
                                services[srv_nr].video_bitrate = inputData->getBitrate();
                                services[srv_nr].gop_size = inputData->getGopSize();
                                services[srv_nr].video_output_mode = inputData->getMode();
                                strncpy(services[srv_nr].video_shm_name, inputData->getFilename().c_str(), 32);
                            }
                            if (stream_type == 17){
                                int audio_stream = services[srv_nr].nr_audio_streams;
                                services[srv_nr].audio_bitrate[audio_stream] = inputData->getBitrate();
                                services[srv_nr].nr_channels[audio_stream] = inputData->getAudioLayout();
                                services[srv_nr].audio_mapping[audio_stream] = inputData->getAudioMapping();
                                strncpy (services[srv_nr].audio_shm_name[audio_stream], inputData->getFilename().c_str(), 32);
                                services[srv_nr].nr_audio_streams++;
                            }

                        }
                        ++itPrj;
                    }
                }
                ++it;
            }
            ++itPmt;

            if (nr_services <= srv_nr)
                nr_services = srv_nr + 1;

            srv_nr++;
        }
    }

    tvltx_info("Inputs: %d\nServices: %d\n", nr_inputs, nr_services);

    // run worker threads (set before the handlers, so an early Ctrl+C sticks)
    running = true;

    // signal handlers. SIGPIPE keeps its default: a dead x264 then stops the
    // whole transmitter (for its supervisor to restart) instead of leaving it
    // on air without video, and x264 children (which inherit an ignored
    // SIGPIPE across exec) still exit once tvltx is gone.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigfunc;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    // do the verifications for error cheking... TODO

    // initialize buffers
    for (int i = 0; i < nr_services; i++){
        initialize_buffer(&services[i].buf_video_in, 29);
        initialize_buffer(&services[i].buf_video_out, 28);
        for (int j = 0; j < services[i].nr_audio_streams; j++){
            initialize_buffer(&services[i].buf_audio_in[j], 26);
            initialize_buffer(&services[i].buf_audio_out[j], 26);
        }
    }

    // start inputs
    for (int i = 0; i < nr_inputs; i++){
        input_start(&inputs[i]);
    }

    // start services
    for (int i = 0; i < nr_services; i++){
        encoder_start(&services[i]);
        output_start(&services[i]);
    }

    return EXIT_SUCCESS;

}

/* Only async-signal-safe work here: flag every thread to stop (they all
 * poll `running`) and let main() shut down in order. A second SIGINT or
 * SIGTERM exits immediately. */
static volatile sig_atomic_t stop_requests = 0;

static void sigfunc(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        if (stop_requests++ == 0) {
            static const char msg[] = "\nStopping... (press Ctrl+C again to exit at once)\n";
            running = false;
            if (write(STDERR_FILENO, msg, sizeof(msg) - 1) < 0) { /* nothing to do */ }
        } else {
            _exit(EXIT_FAILURE);
        }
    }
    /* SIGHUP: reserved for configuration reload, ignored for now */
}
