#include "WebrtcAEC3.h"

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"
#include "webrtc/modules/audio_processing/aec/aec_core_internal.h"

#include <iostream>
#include <stdexcept>


// Helper function for make_unique (C++11 compatibility)
template<typename T, typename... Args>
std::unique_ptr<T> make_unique_helper(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

using namespace webrtc;

WebrtcAEC3::WebrtcAEC3()
    : sample_rate_(48000)
    , system_delay_ms_(8)
    , noise_suppression_level_(1)
    , aec_level_(2)
    , enable_aec_(true)
    , enable_agc_(true)
    , enable_hp_filter_(true)
    , enable_noise_suppression_(true)
    , enable_transient_suppression_(false)
    , aec_delay_agnostic_(false)
    , aec_extended_filter_(false)
    , enable_voice_detection_(true)
    , num_chunk_samples_(0)
    , is_started_(false) {
}

WebrtcAEC3::~WebrtcAEC3() {
}

void WebrtcAEC3::setConfig(int configId, ConfigValue value) {
    if (is_started_) {
        throw std::runtime_error("Cannot change configuration after start() has been called");
    }

    switch (configId) {
    case SAMPLE_RATE:
        if (value.type != ConfigValue::INT) {
            throw std::invalid_argument("SAMPLE_RATE expects int value");
        }
        sample_rate_ = value.int_val;
        break;
    case SYSTEM_DELAY_MS:
        if (value.type != ConfigValue::INT) {
            throw std::invalid_argument("SYSTEM_DELAY_MS expects int value");
        }
        system_delay_ms_ = value.int_val;
        break;
        //        case NOISE_SUPPRESSION_LEVEL:
        //            if (value.type != ConfigValue::INT) {
        //                throw std::invalid_argument("NOISE_SUPPRESSION_LEVEL expects int value");
        //            }
        //            noise_suppression_level_ = value.int_val;
        //            break;
    case NOISE_SUPPRESSION_LEVEL:
        if (value.type != ConfigValue::INT) {
            throw std::invalid_argument("NOISE_SUPPRESSION_LEVEL expects int value");
        }
        if (value.int_val < 0 || value.int_val > 3) {
            throw std::invalid_argument("NOISE_SUPPRESSION_LEVEL must be between 0 and 3");
        }
        noise_suppression_level_ = value.int_val;
        break;

    case AEC_LEVEL:
        if (value.type != ConfigValue::INT) {
            throw std::invalid_argument("AEC_LEVEL expects int value");
        }
        aec_level_ = value.int_val;
        break;
    case ENABLE_AEC:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_AEC expects bool value");
        }
        enable_aec_ = value.bool_val;
        break;
    case ENABLE_AGC:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_AGC expects bool value");
        }
        enable_agc_ = value.bool_val;
        break;
    case ENABLE_HP_FILTER:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_HP_FILTER expects bool value");
        }
        enable_hp_filter_ = value.bool_val;
        break;
    case ENABLE_NOISE_SUPPRESSION:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_NOISE_SUPPRESSION expects bool value");
        }
        enable_noise_suppression_ = value.bool_val;
        break;

    case ENABLE_TRANSIENT_SUPPRESSION:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_TRANSIENT_SUPPRESSION expects bool value");
        }
        enable_transient_suppression_ = value.bool_val;
        break;

    case AEC_DELAY_AGNOSTIC:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("AEC_DELAY_AGNOSTIC expects bool value");
        }
        aec_delay_agnostic_ = value.bool_val;
        break;
    case AEC_EXTENDED_FILTER:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("AEC_EXTENDED_FILTER expects bool value");
        }
        aec_extended_filter_ = value.bool_val;
        break;
    case ENABLE_VOICE_DETECTION:
        if (value.type != ConfigValue::BOOL) {
            throw std::invalid_argument("ENABLE_VOICE_DETECTION expects bool value");
        }
        enable_voice_detection_ = value.bool_val;
        break;

    case AGC_MODE:
        if (value.type != ConfigValue::INT) {
            throw std::invalid_argument("AGC_MODE expects int value");
        }
        if (value.int_val < AGC_MODE_ADAPTIVE_ANALOG || value.int_val > AGC_MODE_FIXED_DIGITAL) {
            throw std::invalid_argument("AGC_MODE must be between 0 and 2");
        }
        agc_mode_ = value.int_val;
        break;

    default:
        throw std::invalid_argument("Invalid configuration ID: " + std::to_string(configId));
    }
}

void WebrtcAEC3::start() {
    if (is_started_) {
        // Already started
        return;

    }

    // Calculate chunk size (10ms worth of samples)
    num_chunk_samples_ = sample_rate_ / 100;

    // Initialize buffers
    near_float_data_.resize(num_chunk_samples_ * WEBRTC_AEC3_NUM_CHANNELS);
    far_float_data_.resize(num_chunk_samples_ * WEBRTC_AEC3_NUM_CHANNELS);
    out_float_data_.resize(num_chunk_samples_ * WEBRTC_AEC3_NUM_CHANNELS);

    // Initialize channel buffers
    near_chan_buf_ = make_unique_helper<ChannelBuffer<float>>(num_chunk_samples_, WEBRTC_AEC3_NUM_CHANNELS);
    far_chan_buf_ = make_unique_helper<ChannelBuffer<float>>(num_chunk_samples_, WEBRTC_AEC3_NUM_CHANNELS);
    out_chan_buf_ = make_unique_helper<ChannelBuffer<float>>(num_chunk_samples_, WEBRTC_AEC3_NUM_CHANNELS);

    // Initialize stream configs
    stream_config_in_ = make_unique_helper<StreamConfig>(sample_rate_, WEBRTC_AEC3_NUM_CHANNELS);
    stream_config_out_ = make_unique_helper<StreamConfig>(sample_rate_, WEBRTC_AEC3_NUM_CHANNELS);

    // Configure audio processing
    configureProcessing();

    is_started_ = true;
}

void WebrtcAEC3::configureProcessing() {
    // Create base configuration
    Config config;
    config.Set<ExperimentalNs>(new ExperimentalNs(enable_transient_suppression_));


    // Create AudioProcessing instance
    audio_processor_ = std::shared_ptr<AudioProcessing>(AudioProcessing::Create(config));

    // Set extra configuration options
    Config extraconfig;
    extraconfig.Set<DelayAgnostic>(new DelayAgnostic(aec_delay_agnostic_));
    extraconfig.Set<ExtendedFilter>(new ExtendedFilter(aec_extended_filter_));
    extraconfig.Set<EchoCanceller3>(new EchoCanceller3(true));
    audio_processor_->SetExtraOptions(extraconfig);

    // Configure Echo Cancellation
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->echo_cancellation()->Enable(enable_aec_));
    if (enable_aec_) {
        audio_processor_->echo_cancellation()->set_suppression_level(EchoCancellation::kHighSuppression);
        if (aec_level_ != -1) {
            RTC_CHECK_EQ(AudioProcessing::kNoError,
                         audio_processor_->echo_cancellation()->set_suppression_level(
                             static_cast<EchoCancellation::SuppressionLevel>(aec_level_)));
        }
        audio_processor_->echo_cancellation()->enable_metrics(true);
        audio_processor_->echo_cancellation()->enable_delay_logging(true);
    }

    // Configure Noise Suppression
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->noise_suppression()->Enable(enable_noise_suppression_));

    if (enable_noise_suppression_) {
        if (noise_suppression_level_ < NS_LEVEL_LOW || noise_suppression_level_ > NS_LEVEL_VERY_HIGH) {
            noise_suppression_level_ = NS_LEVEL_MODERATE;
            std::cerr << "[NS] Invalid level provided. Falling back to MODERATE." << std::endl;
        }

        RTC_CHECK_EQ(AudioProcessing::kNoError,
                     audio_processor_->noise_suppression()->set_level(
                         static_cast<NoiseSuppression::Level>(noise_suppression_level_)));

        std::cout << "[NS] Noise Suppression enabled. Level: " << noise_suppression_level_ << std::endl;
    } else {
        std::cout << "[NS] Noise Suppression disabled." << std::endl;
    }


    // Configure High Pass Filter
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->high_pass_filter()->Enable(enable_hp_filter_));

    // Configure Automatic Gain Control
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->gain_control()->Enable(enable_agc_));

    if (enable_agc_) {
        if (agc_mode_ < AGC_MODE_ADAPTIVE_ANALOG || agc_mode_ > AGC_MODE_FIXED_DIGITAL) {
            agc_mode_ = AGC_MODE_ADAPTIVE_DIGITAL;
            std::cerr << "[AGC] Invalid mode provided. Falling back to ADAPTIVE_DIGITAL." << std::endl;
        }

        RTC_CHECK_EQ(AudioProcessing::kNoError,
                     audio_processor_->gain_control()->set_mode(
                         static_cast<GainControl::Mode>(agc_mode_)));

        // AGC setting
        audio_processor_->gain_control()->set_target_level_dbfs(3);
        audio_processor_->gain_control()->set_compression_gain_db(9);
        audio_processor_->gain_control()->enable_limiter(true);

        std::cout << "[AGC] Enabled. Mode: " << agc_mode_ << std::endl;
    } else {
        std::cout << "[AGC] Disabled." << std::endl;
    }

    // Configure Voice Detection
    if (enable_voice_detection_) {
        audio_processor_->voice_detection()->Enable(enable_voice_detection_);
        audio_processor_->voice_detection()->set_likelihood(VoiceDetection::kModerateLikelihood);
        audio_processor_->voice_detection()->set_frame_size_ms(10);
    }
}

void WebrtcAEC3::validateInputSizes(const std::vector<int16_t>& near_in,
                                    const std::vector<int16_t>& far_in) const {
    if (near_in.size() != num_chunk_samples_) {
        throw std::invalid_argument("near_in size (" + std::to_string(near_in.size()) +
                                    ") does not match expected size (" + std::to_string(num_chunk_samples_) + ")");
    }
    if (far_in.size() != num_chunk_samples_) {
        throw std::invalid_argument("far_in size (" + std::to_string(far_in.size()) +
                                    ") does not match expected size (" + std::to_string(num_chunk_samples_) + ")");
    }
}

void WebrtcAEC3::process(const std::vector<int16_t>& near_in,
                         const std::vector<int16_t>& far_in,
                         std::vector<int16_t>& out) {
    if (!is_started_) {
        throw std::runtime_error("WebrtcAEC3 must be started before processing");
    }

    validateInputSizes(near_in, far_in);

    // Resize output vector
    out.resize(num_chunk_samples_);

    // Convert far-end input from int16 to float
    S16ToFloat(far_in.data(), far_in.size(), far_float_data_.data());
    // Since we're mono, no deinterleaving needed - just copy to channel buffer
    std::copy(far_float_data_.begin(), far_float_data_.end(), far_chan_buf_->channels()[0]);

    // Convert near-end input from int16 to float
    S16ToFloat(near_in.data(), near_in.size(), near_float_data_.data());
    // Since we're mono, no deinterleaving needed - just copy to channel buffer
    std::copy(near_float_data_.begin(), near_float_data_.end(), near_chan_buf_->channels()[0]);

    // Set system delay
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->set_stream_delay_ms(system_delay_ms_));

    // Process reverse stream (far-end/reference signal)
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->ProcessReverseStream(far_chan_buf_->channels(),
                                                        *stream_config_in_,
                                                        *stream_config_out_,
                                                        far_chan_buf_->channels()));

    // Process forward stream (near-end/microphone signal)
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 audio_processor_->ProcessStream(near_chan_buf_->channels(),
                                                 *stream_config_in_,
                                                 *stream_config_out_,
                                                 out_chan_buf_->channels()));

    // Since we're mono, no interleaving needed - just copy from channel buffer
    std::copy(out_chan_buf_->channels()[0],
            out_chan_buf_->channels()[0] + num_chunk_samples_,
            out_float_data_.begin());

    if (!hasVoice()) {
        std::fill(out_float_data_.begin(), out_float_data_.end(), 0.0f);
    }

    // Convert output from float to int16
    FloatToS16(out_float_data_.data(), out.size(), out.data());
}

bool WebrtcAEC3::hasVoice() const {
    if (!is_started_ || !enable_voice_detection_) {
        return false;
    }
    return audio_processor_->voice_detection()->stream_has_voice();
}

bool WebrtcAEC3::hasEcho() const {
    if (!is_started_ || !enable_aec_) {
        return false;
    }
    return audio_processor_->echo_cancellation()->stream_has_echo();
}

float WebrtcAEC3::getSpeechProbability() const {
    if (!is_started_ || !enable_noise_suppression_) {
        return 0.0f;
    }
    return audio_processor_->noise_suppression()->speech_probability();
}


