#include "daisysp.h"
#include "daisy.h"
#include "daisy_seed.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

using namespace daisysp;
using namespace daisy;

class TapeLoop
{
  public:
    static const int MAX_SIZE
        = 32000
          * 10; //TODO 32000 samples/s * 60s/m * 2m = 2 minutes of sample at 32khz.
    float SAMPLE_RATE;

    TapeLoop(float (&track_location)[2][MAX_SIZE]) : track(track_location) {}

    void Init(float sample_rate)
    {
        SAMPLE_RATE = sample_rate;

        blank_track();

        tape_noise.Init();
        tape_noise.SetAmp(tape_noise_amt);

        rdm          = std::default_random_engine(System::GetNow());
        dist_uniform = std::uniform_real_distribution<float>(0.f, 1.f);
        dist_normal
            = std::normal_distribution<float>(demag_size_mean, demag_size_sd);
    }

    void update_tape_speed(){
        float time_const = 48000 * 0.1; 
        tape_speed    = tape_speed + (tape_speed_target - tape_speed) / time_const;
    }

    void advanceTapeHeads()
    {
       update_tape_speed();

        read_head       = fmod((read_head + tape_speed), static_cast<float>(MAX_SIZE));
        last_write_head = write_head;
        write_head      = fmod((read_head - 1.), static_cast<float>(MAX_SIZE));
    }

    int circular_distance(int a, int b)
    {
        int direct = std::abs(a - b);   // straight distance
        int wrap   = MAX_SIZE - direct; // distance if you wrap around
        return std::min(direct, wrap);
    }

    float linear_interpolate(float a, float b, float fraction)
    {
        return ((b - a) * fraction + a);
    }

    std::vector<float> readNextSamples()
    {

        std::vector<float> out = {0.0f, 0.0f};
        // advanceTapeHeads();
        int read_head_whole = static_cast<int>(
            read_head); //the whole integer part of the read head location
        float read_head_fractional
            = read_head
              - static_cast<float>(
                  read_head_whole); // the fractional portion of the read head location

        int next_read_head
            = ((tape_speed > 0 ? read_head_whole + 1 : read_head_whole - 1)
               + MAX_SIZE)
              % MAX_SIZE;

        float interpolated_L = linear_interpolate(track[0][read_head_whole],
                                                  track[0][next_read_head],
                                                  read_head_fractional);
        float interpolated_R = linear_interpolate(track[1][read_head_whole],
                                                  track[1][next_read_head],
                                                  read_head_fractional);

        out[0] = interpolated_L;
        out[1] = interpolated_R;

        return out;
    }

    /* This Method will NOT move tape heads and writes to the write_head location.
It's also the actual coded method, the overloaded method with a vector just uses this. */

    void writeNextSamples(std::vector<float> samples)
    {
        int  wh         = static_cast<int>(write_head);
        int  lwh        = static_cast<int>(last_write_head);
        bool is_reverse = tape_speed < 0;

        if(track[0][wh] >= -100.f)
        {
            track[0][wh] = samples[0];
        }
        if(track[1][wh] >= -100.f)
        {
            track[1][wh] = samples[1];
        }

        //TODO debug this
        int steps = circular_distance(wh, lwh);
        for(int step = 1; step < steps; step++)
        {
            int i = (lwh + (is_reverse ? -step : step) + MAX_SIZE) % MAX_SIZE;

            if(!is_reverse)
            {
                i = (i + 1 + MAX_SIZE) % MAX_SIZE;
            }
            else
            {
                i = (i - 1 + MAX_SIZE) % MAX_SIZE;
            }

            float a1   = track[0][lwh];
            float a2   = track[1][lwh];
            float b1   = track[0][wh];
            float b2   = track[1][wh];
            float frac = static_cast<float>(step) / static_cast<float>(steps);

            track[0][i] = linear_interpolate(a1, b1, frac);
            track[1][i] = linear_interpolate(a2, b2, frac);
        }

        end_loc = write_head;
    }


    void blank_track()
    {
        for(int i = 0; i < MAX_SIZE; i++)
        {
            track[0][i] = 0.f;
            track[1][i] = 0.f;
        }
        start_loc = 0;
        filled    = false;
    }


    void apply_tape_artifact()
    {   //TODO for some reason I'm not hearing pops?
        /*This simulates de-magnetization of the tape. theres a demag_probability chance 
of losing a group of bits. The number of bits is given by a normal distribution.*/

        for(int i = 0; i < MAX_SIZE; i++)
        {
            int bits_lost = 0;
            if(dist_uniform(rdm) < demag_probability)
            {
                bits_lost = static_cast<int>(std::round(dist_normal(rdm)));
            }
            while(bits_lost > 0)
            {
                track[0][i] = std::numeric_limits<float>::lowest();
                track[1][i] = std::numeric_limits<float>::lowest();

                i++;
                bits_lost--;
            }
        }
        for(unsigned i = 0; i < tape_splice_gap; i++)
        {
            track[0][i] = std::numeric_limits<float>::lowest();
            track[1][i] = std::numeric_limits<float>::lowest();
        }
    }

    template <typename T>
    const T& clamp(const T& value, const T& low, const T& high)
    {
        return (value < low) ? low : (value > high) ? high : value;
    }

    void next_Input_Mode() { input_mode = (input_mode + 1) % num_input_modes; }

    void set_Input_Mode(int m) { input_mode = m; }

    int get_Input_Mode() { return input_mode; }

    int get_Read_Head() { return static_cast<int>(read_head); }

    int get_Tape_Speed() { return static_cast<int>(tape_speed * 100.f); }

    //** Takes an input float between 0. and 1.0, sets the tape speed according to TapeLoop::Min/Max_tape_speed */
    void set_Playback_Speed(float speed_decimal)
    {
        tape_speed_target = speed_decimal * (max_tape_speed - min_tape_speed)
                            - max_tape_speed;
    }

    int get_Target_Speed()
    {
        return static_cast<int>(tape_speed_target * 100.f);
    }

    float get_Volume() { return volume; }

    void set_Volume(float v) { volume = v; }

    float get_Fade() { return fade; }

    void set_Fade(float input) { fade = input; }

  private:
    float (&track)[2][MAX_SIZE];
    bool is_blank_track = true;
    int  start_loc      = 0;
    bool filled         = false;
    int  end_loc        = 0;

    // bool is_playing = false;
    // bool is_recording = false;

    float read_head       = 0.;
    float write_head      = MAX_SIZE - 1;
    float last_write_head = write_head;

    int input_mode      = 0;
    int num_input_modes = 2;

    float       tape_speed_target = 0.f;
    float       tape_speed        = 0.f;
    const float min_tape_speed    = -2.f;
    const float max_tape_speed    = 2.f;

    float volume = 0.5;
    float fade   = 0.5;

    //tape preparation
    WhiteNoise            tape_noise;
    float                 tape_noise_amt = 0.002f; // tape noise amplitude
    static const unsigned tape_splice_gap
        = 0.5 / 47.5
          * 32000; // =0.5mm tape gap / 47.5 mm/s feed rate * 32000 samples/second
    static constexpr float demag_probability = 0.1f;
    static const int       demag_size_mean   = 6.f;
    static const int       demag_size_sd     = 2.f;

    std::default_random_engine            rdm;
    std::uniform_real_distribution<float> dist_uniform;
    std::normal_distribution<float>       dist_normal;
};