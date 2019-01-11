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
    int deletedPoints = 0,secos;
    double minDeletedPower = 0.0, maxDeletedPower = 0.0, lastPointSeconds = -999.0;
    bool wrongPowerRange = false;

    std::vector<int> deletedSecs;

    // put it all in a LUW
    ride->command->startLUW("Remove Bad Start Values");

    for (int position = 0; position < ride->dataPoints().count(); position++) {
        RideFilePoint *point = ride->dataPoints()[position];

        secos = static_cast<int>(round(point->secs));

        //XDataSeries* a = ride->xdata("DEVELOPER");
        //XDataPoint* b = a->datapoints[position];
        //b->secs;
        //if (NULL != last) {

        if (point->watts == 0.0){
            ride->command->deletePoint(position);
            deletedSecs.push_back(secos);
            printf("Deleted point at %d sec because of power 0 \n",secos);
            deletedPoints++;
            position--;
            continue;
        }

        // Pause detected
        if (point->secs > (lastPointSeconds + ride->recIntSecs())){
            printf("Detected gap at %d sec \n",secos);
            
            wrongPowerRange = point->watts < 120.0;

            if (wrongPowerRange){
                deletedPoints++;

                ride->command->deletePoint(position);
                deletedSecs.push_back(secos);
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

    }

     for (int position = 0; position < ride->xdata("DEVELOPER")->datapoints.count(); position++) {
         XDataPoint *xPoint = ride->xdata("DEVELOPER")->datapoints[position];
         int xsecos = static_cast<int>(round(xPoint->secs));
         if( std::find(deletedSecs.begin(), deletedSecs.end(), xsecos) != deletedSecs.end() ){
            printf("Deleted xPoint at %d because its in the list \n",xsecos);
            ride->command->deleteXDataPoints("DEVELOPER",position,1);
            position--;
         }
     }


    // end the Logical unit of work here
    ride->command->endLUW();

    ride->setTag("Deleted Data Points", QString("%1").arg(deletedPoints));
    ride->setTag("Deleted Power Range Time", QString("%1 - %2").arg(minDeletedPower).arg(maxDeletedPower));

    return deletedPoints != 0;
}
