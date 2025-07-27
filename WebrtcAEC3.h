#ifndef WEBRTC_AEC3_H
#define WEBRTC_AEC3_H

#include <vector>
#include <memory>
#include <cstdint>

// Constants
#define WEBRTC_AEC3_NUM_CHANNELS 1

// Union-based variant replacement for C++11 compatibility
struct ConfigValue {
    enum Type { INT, BOOL, FLOAT };
    Type type;
    union {
        int int_val;
        bool bool_val;
        float float_val;
    };

    ConfigValue(int val) : type(INT), int_val(val) {}
    ConfigValue(bool val) : type(BOOL), bool_val(val) {}
    ConfigValue(float val) : type(FLOAT), float_val(val) {}
};

// Forward declarations for WebRTC types
namespace webrtc {
    class AudioProcessing;
    template<typename T> class ChannelBuffer;
    struct StreamConfig;
}

class WebrtcAEC3 {
public:
    // Configuration IDs
    enum ConfigId {
        SAMPLE_RATE = 0,
        SYSTEM_DELAY_MS = 1,
        NOISE_SUPPRESSION_LEVEL = 2,
        AEC_LEVEL = 3,
        ENABLE_AEC = 4,
        ENABLE_AGC = 5,
        ENABLE_HP_FILTER = 6,
        ENABLE_NOISE_SUPPRESSION = 7,
        ENABLE_TRANSIENT_SUPPRESSION = 8,
        AEC_DELAY_AGNOSTIC = 9,
        AEC_EXTENDED_FILTER = 10,
        ENABLE_VOICE_DETECTION = 11
    };

    WebrtcAEC3();
    ~WebrtcAEC3();

    void setConfig(int configId, ConfigValue value);
    void start();
    void process(const std::vector<int16_t>& near_in,
                const std::vector<int16_t>& far_in,
                std::vector<int16_t>& out);

    // Optional: Get processing statistics
    bool hasVoice() const;
    bool hasEcho() const;
    float getSpeechProbability() const;

private:
    void configureProcessing();
    void validateInputSizes(const std::vector<int16_t>& near_in,
                           const std::vector<int16_t>& far_in) const;

    // Configuration parameters
    int sample_rate_;
    int system_delay_ms_;
    int noise_suppression_level_;
    int aec_level_;
    bool enable_aec_;
    bool enable_agc_;
    bool enable_hp_filter_;
    bool enable_noise_suppression_;
    bool enable_transient_suppression_;
    bool aec_delay_agnostic_;
    bool aec_extended_filter_;
    bool enable_voice_detection_;

    // WebRTC objects
    std::shared_ptr<webrtc::AudioProcessing> audio_processor_;
    std::unique_ptr<webrtc::StreamConfig> stream_config_in_;
    std::unique_ptr<webrtc::StreamConfig> stream_config_out_;

    // Processing buffers
    std::vector<float> near_float_data_;
    std::vector<float> far_float_data_;
    std::vector<float> out_float_data_;
    std::unique_ptr<webrtc::ChannelBuffer<float>> near_chan_buf_;
    std::unique_ptr<webrtc::ChannelBuffer<float>> far_chan_buf_;
    std::unique_ptr<webrtc::ChannelBuffer<float>> out_chan_buf_;

    // Processing parameters
    size_t num_chunk_samples_;
    bool is_started_;
};

#endif // WEBRTC_AEC3_H


