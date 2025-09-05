#include "daisy_seed.h"
#include "daisysp.h"
#include "System.h"
#include "TapeLoop.h"
#include <stdlib.h>
#include <math.h>

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;


DaisySeed        hw;
const int        num_adc_channels = 3;
AdcChannelConfig adc_config[num_adc_channels];
enum AdcChannel
{
    MUX         = 0,
    QUALITY_POT = 1,
    WARBLE_POT  = 2
};

enum MUX_IDX
{
    VOL1 = 3,
    VOL2 = 0,
    VOL3 = 1,
    VOL4 = 2,
    TC1  = 5,
    TC2  = 7,
    TC3  = 6,
    TC4  = 4

};

Pin MUX_A_PIN   = D18;
Pin MUX_B_PIN   = D19;
Pin MUX_C_PIN   = D20;
Pin MUX_OUT_PIN = A2;
Pin MUX_INH_PIN = D21;

//TODO need to define MUX pins 1-8

Pin QUALITY_PIN = A0;
Pin WARBLE_PIN  = A1;

Pin trk1Sel_pin = D1;
Pin trk2Sel_pin = D2;
Pin trk3Sel_pin = D3;
Pin trk4Sel_pin = D4;

Pin trk1LED_pin = D5;
Pin trk2LED_pin = D6;
Pin trk3LED_pin = D7;
Pin trk4LED_pin = D8;

Pin ply_pin = D11;
Pin rcd_pin = D12;

Switch track1_sel;
Switch track2_sel;
Switch track3_sel;
Switch track4_sel;


Switch play_pause_button;
Switch record_button;

Led   track1_LED;
float track1_LED_Brightness = 1.f;
Led   track2_LED;
float track2_LED_Brightness = 1.f;
Led   track3_LED;
float track3_LED_Brightness = 1.f;
Led   track4_LED;
float track4_LED_Brightness = 1.f;

Led LEDs[4]{};

float DSY_SDRAM_BSS track1_loc[2][TapeLoop::MAX_SIZE];
TapeLoop            track1(track1_loc);
float DSY_SDRAM_BSS track2_loc[2][TapeLoop::MAX_SIZE];
TapeLoop            track2(track2_loc);
float DSY_SDRAM_BSS track3_loc[2][TapeLoop::MAX_SIZE];
TapeLoop            track3(track3_loc);
float DSY_SDRAM_BSS track4_loc[2][TapeLoop::MAX_SIZE];
TapeLoop            track4(track4_loc);

int active_track = 0;

TapeLoop tracks[4] = {track1, track2, track3, track4}; //Expand these

enum TRACK_MODES
{
    TRACK_SPEED = 0,
    TRACK_FADE  = 1,

};


float quality = 1.f;
float warble  = 0.f;

void tracks_Init()
{
    track1.Init(hw.AudioSampleRate());
    track2.Init(hw.AudioSampleRate());
    track3.Init(hw.AudioSampleRate());
    track4.Init(hw.AudioSampleRate());
}

/* 

*/

float saturate_and_compress(float value)
{ //TODO tweak this???
    float saturation = 30 * ((1 - quality) * (1.f - 0.1) + 0.1);
    float saturated  = atan(saturation * value);
    return saturated;
}

void peripherals_Init()
{
    //INS
    track1_sel.Init(trk1Sel_pin,
                    0.0f,
                    Switch::Type::TYPE_MOMENTARY,
                    Switch::Polarity::POLARITY_INVERTED,
                    Switch::Pull::PULL_UP);
    track2_sel.Init(trk2Sel_pin,
                    0.0f,
                    Switch::Type::TYPE_MOMENTARY,
                    Switch::Polarity::POLARITY_INVERTED,
                    Switch::Pull::PULL_UP);
    track3_sel.Init(trk3Sel_pin,
                    0.0f,
                    Switch::Type::TYPE_MOMENTARY,
                    Switch::Polarity::POLARITY_INVERTED,
                    Switch::Pull::PULL_UP);
    track4_sel.Init(trk4Sel_pin,
                    0.0f,
                    Switch::Type::TYPE_MOMENTARY,
                    Switch::Polarity::POLARITY_INVERTED,
                    Switch::Pull::PULL_UP);
    play_pause_button.Init(ply_pin, 0.f, Switch::Type::TYPE_TOGGLE, Switch::Polarity::POLARITY_NORMAL, Switch::PULL_DOWN);
    record_button.Init(rcd_pin, 0.f, Switch::Type::TYPE_TOGGLE, Switch::Polarity::POLARITY_NORMAL, Switch::PULL_DOWN);

    //OUTS
    track1_LED.Init(trk1LED_pin, false, 2000.f);
    track2_LED.Init(trk2LED_pin, false);
    track3_LED.Init(trk3LED_pin, false);
    track4_LED.Init(trk4LED_pin, false);
    LEDs[0] = track1_LED;
    LEDs[1] = track2_LED;
    LEDs[2] = track3_LED;
    LEDs[3] = track4_LED;

    //Display
    //ADC
    adc_config[MUX].InitMux(MUX_OUT_PIN, 8, MUX_A_PIN, MUX_B_PIN, MUX_C_PIN);
    adc_config[QUALITY_POT].InitSingle(QUALITY_PIN);
    adc_config[WARBLE_POT].InitSingle(WARBLE_PIN);

    hw.adc.Init(adc_config, num_adc_channels);
    hw.adc.Start();
}

void blink(int led, int nTimes)
{
    while(nTimes + 1 > 0)
    {
        LEDs[led].Set(0.f);
        LEDs[led].Update();
        hw.DelayMs(100);
        LEDs[led].Set(1.f);
        LEDs[led].Update();
        hw.DelayMs(100);
        nTimes--;
    }
}

void update_track(int t, float tc)
{
	play_pause_button.Debounce();
    switch(tracks[t].get_Input_Mode())
    {
        case TRACK_SPEED:
            tracks[t].set_Playback_Speed(play_pause_button.Pressed() ? tc : 0.5);
            break;

			case TRACK_FADE: tracks[t].set_Fade(tc); break;
    }
}


void update_buttons()
{
    track1_sel.Debounce();
    if(track1_sel.RisingEdge())
    {
        if(active_track == 0)
        {
            tracks[active_track].next_Input_Mode();
            blink(0, tracks[active_track].get_Input_Mode());
        }
        else
        {
            active_track = 0;
        }
    }

    track2_sel.Debounce();
    if(track2_sel.RisingEdge())
    {
        if(active_track == 1)
        {
            tracks[active_track].next_Input_Mode();
            blink(1, tracks[active_track].get_Input_Mode());
        }
        else
        {
            active_track = 1;
        }
    }

    track3_sel.Debounce();
    if(track3_sel.RisingEdge())
    {
        if(active_track == 2)
        {
            tracks[active_track].next_Input_Mode();
            blink(2, tracks[active_track].get_Input_Mode());
        }
        else
        {
            active_track = 2;
        }
    }

    track4_sel.Debounce();
    if(track4_sel.RisingEdge())
    {
        if(active_track == 3)
        {
            tracks[active_track].next_Input_Mode();
            blink(3, tracks[active_track].get_Input_Mode());
        }
        else
        {
            active_track = 3;
        }
    }
}

void update_pots()
{ //TODO fix this for PCB implementation

tracks[0].set_Volume(hw.adc.GetMuxFloat(MUX, VOL1));
tracks[1].set_Volume(hw.adc.GetMuxFloat(MUX, VOL2));
tracks[2].set_Volume(hw.adc.GetMuxFloat(MUX, VOL3));
tracks[3].set_Volume(hw.adc.GetMuxFloat(MUX, VOL4));


update_track(0, (1.f - hw.adc.GetMuxFloat(MUX, TC1)));
update_track(1, (1.f - hw.adc.GetMuxFloat(MUX, TC2)));
update_track(2, (1.f - hw.adc.GetMuxFloat(MUX, TC3)));
update_track(3, (1.f - hw.adc.GetMuxFloat(MUX, TC4)));
    
    quality = hw.adc.GetFloat(QUALITY_POT);
    warble  = hw.adc.GetFloat(WARBLE_POT);
}

void update_LEDs()
{
    int speed = 2;
    for(int i = 0; i < 4; i++)
    {
        LEDs[i].Set(sin(tracks[i].get_Read_Head() * speed / hw.AudioSampleRate()) + 1.1);
        LEDs[i].Update();
    }
}

void update()
{
    update_buttons();
    update_pots();
    update_LEDs();
}

// std::vector<float> fade(std::vector<float> samps){

// }

// std::vector<float> mix_tracks(std::vector<float> t1, std::vector<float> t2, std::vector<float> t3, std::vector<float> t4){
// 	float left = t1[0] + t2[0] + t3[0] + t4[0];
// 	float right = t1[1] + t2[1] + t3[1] + t4[1];
// 	return {left/5.f, right/5.f};
// }


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        tracks[0].advanceTapeHeads();
        tracks[1].advanceTapeHeads();
        tracks[2].advanceTapeHeads();
        tracks[3].advanceTapeHeads();


        std::vector<float> mixed   = {0.f, 0.f};
        std::vector<float> monitor = {in[0][i], in[1][i]};

        std::vector<float> samps;
        for(uint8_t i = 0; i < 4; i++)
        {
            float f = tracks[i].get_Fade();
            float v = tracks[i].get_Volume();

            samps = tracks[i].readNextSamples();

            mixed[0] = mixed[0] + (samps[0] * (1.f - f)) * v;
            mixed[1] = mixed[1] + (samps[1] * f) * v;
        }

        out[0][i] = mixed[0] * 0.75 + monitor[0] * .25;
        out[1][i] = mixed[1] * 0.75 + monitor[1] * .25;


		record_button.Debounce();
        if(record_button.Pressed())
        {
            tracks[active_track].writeNextSamples({out[0][i], out[1][i]});
        }

		out[0][i] = saturate_and_compress(out[0][i]);
        out[1][i] = saturate_and_compress(out[1][i]);

    }
}

void print_all()
{ //this is for debugging and will be deleted TODO


	// hw.PrintLine("PLY\tTC\n%d\t%d", play_pause_button.Pressed(), static_cast<int>(hw.adc.GetMuxFloat(MUX, TC1)*100.f));

    // hw.PrintLine("TC\tSP\n%d\t%d", static_cast<int>(hw.adc.GetMuxFloat(MUX, TC1)*100.f), static_cast<int>(track1.get_Tape_Speed()));

hw.PrintLine("TRK\tV\tV_in\n%d\t%d\t%d",
				active_track,
				static_cast<int>(tracks[active_track].get_Volume()*100),
				static_cast<int>(hw.adc.GetMuxFloat(MUX, VOL1)*100));
				
}


int main(void)
{
    hw.Init();
    bool debug = false;

    tracks_Init();
    peripherals_Init();

    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAudio(AudioCallback);

    if(debug)
    {
        hw.StartLog(true);
    }

    while(1)
    {
        update();


        if(debug)
        {
            print_all();
        }
    }
}
