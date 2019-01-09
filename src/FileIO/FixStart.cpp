/*
 * Copyright (c) 2010 Hennadiy Verkh (dev@verkh.de)
 * Based on Mark Liversedge's FixGaps.cpp
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "DataProcessor.h"
#include "Settings.h"
#include "Units.h"
#include "HelpWhatsThis.h"
#include <algorithm>
#include <QVector>

// Config widget used by the Preferences/Options config panes
class FixStart;
class FixStartConfig : public DataProcessorConfig
{
    Q_DECLARE_TR_FUNCTIONS(FixStartConfig)

    friend class ::FixStart;
    protected:
        QHBoxLayout *layout;
        QLabel *secondsToProcessLabel;
        QDoubleSpinBox *secondsToProcess;

    public:
        FixStartConfig(QWidget *parent) : DataProcessorConfig(parent) {

            HelpWhatsThis *help = new HelpWhatsThis(parent);
            parent->setWhatsThis(help->getWhatsThisText(HelpWhatsThis::MenuBar_Edit_FixStartInRecording));

            layout = new QHBoxLayout(this);

            layout->setContentsMargins(0,0,0,0);
            setContentsMargins(0,0,0,0);

            secondsToProcessLabel = new QLabel(tr("Seconds to process"));

            secondsToProcess = new QDoubleSpinBox();
            secondsToProcess->setMaximum(99.99);
            secondsToProcess->setMinimum(0);
            secondsToProcess->setSingleStep(0.1);

            layout->addWidget(secondsToProcessLabel);
            layout->addWidget(secondsToProcess);
            layout->addStretch();
        }

        //~FixStartConfig() {} // deliberately not declared since Qt will delete
                              // the widget and its children when the config pane is deleted

        QString explain() {
            return(QString(tr("On activity start, or resume from pause, there"
                              "are bad values for the first N seconds."
                           "This function performs this task, taking two "
                           "parameters;\n\n"
                           "max_seconds - this defines the maximum duration of a"
                           "bad values period that will be deleted. Bad values"
                           "after this period  will not be affected.\n\n")));
        }

        void readConfig() {
            double seconds = appsettings->value(NULL, GC_DPFST_SECONDS, "10.0").toDouble();
            secondsToProcess->setValue(seconds);
        }

        void saveConfig() {
            appsettings->setValue(GC_DPFST_SECONDS, secondsToProcess->value());
        }
};


// RideFile Dataprocessor -- used to insert gaps in recording
//                           by deleting bad values after activity
//                           start/resume
//
class FixStart : public DataProcessor {
    Q_DECLARE_TR_FUNCTIONS(FixStart)

    public:
        FixStart() {}
        ~FixStart() {}

        // the processor
        bool postProcess(RideFile *, DataProcessorConfig* config, QString op);

        // the config widget
        DataProcessorConfig* processorConfig(QWidget *parent) {
            return new FixStartConfig(parent);
        }

        // Localized Name
        QString name() {
            return (tr("Remove Bad Start Values"));
        }
};

static bool fixStartDeleted = DataProcessorFactory::instance().registerProcessor(QString("Remove Bad Start Values"), new FixStart());

bool
FixStart::postProcess(RideFile *ride, DataProcessorConfig *config=0, QString op="")
{
    Q_UNUSED(op)

    // get settings
    double secondsToProcess;
    if (config == NULL) { // being called automatically
        secondsToProcess = appsettings->value(NULL, GC_DPFST_SECONDS, "10.0").toDouble();
    } else { // being called manually
        secondsToProcess = ((FixStartConfig*)(config))->secondsToProcess->value();
    }

    // If there are less than 2 dataPoints then there
    // is no way of post processing anyway (e.g. manual workouts)
    if (ride->dataPoints().count() < 2) return false;

    // OK, so there are probably some gaps, lets post process them
    RideFilePoint *last = NULL;
    int deletedPoints = 0;
    double minDeletedPower = 0.0, maxDeletedPower = 0.0, lastPointSeconds = -999.0;
    bool wrongPowerRange = false;

    // put it all in a LUW
    ride->command->startLUW("Remove Bad Start Values");

    for (int position = 0; position < ride->dataPoints().count(); position++) {
        RideFilePoint *point = ride->dataPoints()[position];

        //if (NULL != last) {

        // Pause detected
        if (point->secs > (lastPointSeconds + ride->recIntSecs())){
            printf("Detected gap at %4.0f sec \n",point->secs);
            
            wrongPowerRange = point->watts < 120.0;

            if (wrongPowerRange){
                deletedPoints++;

                ride->command->deletePoint(position);
                ride->command->deleteXDataPoints("DEVELOPER",position,1);
                position--;
                if (minDeletedPower > point->watts)  minDeletedPower = point->watts;
                if (maxDeletedPower < point->watts)  maxDeletedPower = point->watts;
                printf("Deleted point because of power %4.2f \n",point->watts);


            }else {
                lastPointSeconds = point->secs;
            }
        }else{
            lastPointSeconds = point->secs;
        }

            
/*            double gap = point->secs - last->secs - ride->recIntSecs();

            // if we have gps and we moved, then this isn't a stop
            bool stationary = ((last->lat || last->lon) && (point->lat || point->lon)) // gps is present
                         && last->lat == point->lat && last->lon == point->lon;

            // moved for less than stop seconds ... interpolate
            if (!stationary && gap >= secondsToProcess && gap <= stop) {

                // what's needed?
                deletion++;
                deletionTime += gap;

                int count = gap/ride->recIntSecs();
                double hrdelta = (point->hr - last->hr) / (double) count;
                double pwrdelta = (point->watts - last->watts) / (double) count;
                double kphdelta = (point->kph - last->kph) / (double) count;
                double kmdelta = (point->km - last->km) / (double) count;
                double caddelta = (point->cad - last->cad) / (double) count;
                double altdelta = (point->alt - last->alt) / (double) count;
                double nmdelta = (point->nm - last->nm) / (double) count;
                double londelta = (point->lon - last->lon) / (double) count;
                double latdelta = (point->lat - last->lat) / (double) count;
                double hwdelta = (point->headwind - last->headwind) / (double) count;
                double slopedelta = (point->slope - last->slope) / (double) count;
                double temperaturedelta = (point->temp - last->temp) / (double) count;
                double lrbalancedelta = (point->lrbalance - last->lrbalance) / (double) count;
                double ltedelta = (point->lte - last->lte) / (double) count;
                double rtedelta = (point->rte - last->rte) / (double) count;
                double lpsdelta = (point->lps - last->lps) / (double) count;
                double rpsdelta = (point->rps - last->rps) / (double) count;
                double lpcodelta = (point->lpco - last->lpco) / (double) count;
                double rpcodelta = (point->rpco - last->rpco) / (double) count;
                double lppbdelta = (point->lppb - last->lppb) / (double) count;
                double rppbdelta = (point->rppb - last->rppb) / (double) count;
                double lppedelta = (point->lppe - last->lppe) / (double) count;
                double rppedelta = (point->rppe - last->rppe) / (double) count;
                double lpppbdelta = (point->lpppb - last->lpppb) / (double) count;
                double rpppbdelta = (point->rpppb - last->rpppb) / (double) count;
                double lpppedelta = (point->lpppe - last->lpppe) / (double) count;
                double rpppedelta = (point->rpppe - last->rpppe) / (double) count;
                double smo2delta = (point->smo2 - last->smo2) / (double) count;
                double thbdelta = (point->thb - last->thb) / (double) count;
                double rcontactdelta = (point->rcontact - last->rcontact) / (double) count;
                double rcaddelta = (point->rcad - last->rcad) / (double) count;
                double rvertdelta = (point->rvert - last->rvert) / (double) count;
                double tcoredelta = (point->tcore - last->tcore) / (double) count;


                // add the points
                for(int i=0; i<count; i++) {
                    RideFilePoint *add = new RideFilePoint(last->secs+((i+1)*ride->recIntSecs()),
                                                           last->cad+((i+1)*caddelta),
                                                           last->hr + ((i+1)*hrdelta),
                                                           last->km + ((i+1)*kmdelta),
                                                           last->kph + ((i+1)*kphdelta),
                                                           last->nm + ((i+1)*nmdelta),
                                                           last->watts + ((i+1)*pwrdelta),
                                                           last->alt + ((i+1)*altdelta),
                                                           last->lon + ((i+1)*londelta),
                                                           last->lat + ((i+1)*latdelta),
                                                           last->headwind + ((i+1)*hwdelta),
                                                           last->slope + ((i+1)*slopedelta),
                                                           last->temp + ((i+1)*temperaturedelta),
                                                           last->lrbalance + ((i+1)*lrbalancedelta),
                                                           last->lte + ((i+1)*ltedelta),
                                                           last->rte + ((i+1)*rtedelta),
                                                           last->lps + ((i+1)*lpsdelta),
                                                           last->rps + ((i+1)*rpsdelta),
                                                           last->lpco + ((i+1)*lpcodelta),
                                                           last->rpco + ((i+1)*rpcodelta),
                                                           last->lppb + ((i+1)*lppbdelta),
                                                           last->rppb + ((i+1)*rppbdelta),
                                                           last->lppe + ((i+1)*lppedelta),
                                                           last->rppe + ((i+1)*rppedelta),
                                                           last->lpppb + ((i+1)*lpppbdelta),
                                                           last->rpppb + ((i+1)*rpppbdelta),
                                                           last->lpppe + ((i+1)*lpppedelta),
                                                           last->rpppe + ((i+1)*rpppedelta),
                                                           last->smo2 + ((i+1)*smo2delta),
                                                           last->thb + ((i+1)*thbdelta),
                                                           last->rvert + ((i+1)*rvertdelta),
                                                           last->rcad + ((i+1)*rcaddelta),
                                                           last->rcontact + ((i+1)*rcontactdelta),
                                                           last->tcore + ((i+1)*tcoredelta),
                                                           last->interval);

                    ride->command->insertPoint(position++, add);
                }

            // stationary or greater than 30 seconds... fill with zeroes
            } else if (gap > stop) {

                deletion++;
                deletionTime += gap;

                int count = gap/ride->recIntSecs();
                double kmdelta = (point->km - last->km) / (double) count;

                // add zero value points
                for(int i=0; i<count; i++) {
                    RideFilePoint *add = new RideFilePoint(last->secs+((i+1)*ride->recIntSecs()),
                                                           0,
                                                           0,
                                                           last->km + ((i+1)*kmdelta),
                                                           0,
                                                           0,
                                                           0,
                                                           last->alt,
                                                           0,
                                                           0,
                                                           0,
                                                           0,
                                                           0,
                                                           0,
                                                           0.0, 0.0, 0.0, 0.0, //pedal torque / smoothness
                                                           0.0, 0.0, // pedal platform offset
                                                           0.0, 0.0, 0.0, 0.0, //pedal power phase
                                                           0.0, 0.0, 0.0, 0.0, //pedal peak power phase
                                                           0.0, 0.0, // smO2 / thb
                                                           0.0, 0.0, 0.0, // running dynamics
                                                           0.0,
                                                           last->interval);
                    ride->command->insertPoint(position++, add);
                    //ride->command->deletePoint(position);
                    //ride->command->deletePoints()
                }
            }*/
        //}
        last = point;
    }

    // end the Logical unit of work here
    ride->command->endLUW();

    ride->setTag("Deleted Data Points", QString("%1").arg(deletedPoints));
    ride->setTag("Deleted Power Range Time", QString("%1 - %2").arg(minDeletedPower).arg(maxDeletedPower));

    return deletedPoints != 0;
}
