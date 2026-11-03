/*
    TVLTx - TV Lovers Transmitter, an ISDB-Tb transmitter
    Copyright (C) 2012-2018 Rafael Diniz

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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include "bts_tx.h"

#include "project/XMLProject.h"

output_parameters mod_pars[MAX_OUTPUTS];
int nr_outputs;

int start_outputs(Project *project)
{
    nr_outputs = 0;

    map<int, OutputCardInfo*>::iterator oci;

    oci = project->getOutputCardList()->begin();
    while (oci != project->getOutputCardList()->end()) {
        int id = oci->second->getId();
        if (id >= MAX_OUTPUTS){
            fprintf(stderr, "Error: project output id must be incremental, starting from 0 and smaller then %d\n", MAX_OUTPUTS);
            return EXIT_FAILURE;
        }
        fprintf(stderr, "we have ouput cards..\n");
        mod_pars[id].output_id = id;
        mod_pars[id].card_number = oci->second->getCardNumber();
        mod_pars[id].card_type = oci->second->getBrand();
        mod_pars[id].output_type = oci->second->getType();

        int freq = oci->second->getFrequency();
        if (freq < 100000000 || freq > 999999999){
            fprintf(stderr, "Error: Invalid frequency in Output ID = %d \n", mod_pars[id].output_id);
            ++oci;
            continue;
        }

        char freq_s[32];
        snprintf(freq_s, 32, "%d", freq);
        strncpy(mod_pars[id].freq, freq_s, 3);
        mod_pars[id].freq[3] = '.';
        strcpy(&mod_pars[id].freq[4], &freq_s[3]);

        int power = oci->second->getTxPower();
        snprintf(mod_pars[id].power, 32, "%d", power);

        mod_pars[id].bitrate = oci->second->getBitrate();
        mod_pars[id].packet_size = oci->second->getPacketSize();

        fprintf(stderr, "frequency: %s power: %s\n", mod_pars[id].freq, mod_pars[id].power);

        nr_outputs++;
        ++oci;
    }


    for (int i = 0; i < nr_outputs; i++) {
        initialize_buffer(&mod_pars[i].buffer_out, 29);

        if (mod_pars[i].output_type == OUTPUT_ASI)
        {
            if(pthread_create(&mod_pars[i].output_asi_TID, NULL, output_asi_thread_dektec, &mod_pars[i]))
            {
                fprintf(stderr,"Failed to create Dektec thread.\n");
                return EXIT_FAILURE;
            }
        }

        if (mod_pars[i].output_type == OUTPUT_MODULATION)
        {
            if(pthread_create(&mod_pars[i].output_mod_TID, NULL, output_mod_thread_dektec, &mod_pars[i]))
            {
                fprintf(stderr,"Failed to create Dektec thread.\n");
                return EXIT_FAILURE;
            }
        }
    } // for ()

    return EXIT_SUCCESS;
}
