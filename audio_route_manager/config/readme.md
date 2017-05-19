# Audio Route Manager Configuration

The configuration of Audio HAL is making full reuse of audio policy configuration file
by:
    -enhancing mixPort informations (attributes that would be ignored by the policy)
        Each mixPort will be interpreted as a Stream Route.
    -including custom XML file:
        -list of audio parameter-framework criterion types
        -list of audio parameter-framework criteria
        -list of audio parameter-framework rogue parameters

For more documentation on the PFW, please refer to the github link:
https://github.com/01org/parameter-framework

# Audio Stream Route Example:

    <mixPort name="<Name is also used for Audio PFW Criterion ex: Media>"
             role="<source|sink>"
             flags="<list of affinity of flags: i.e. AUDIO_OUTPUT_FLAG_PRIMARY ("|" separated)>"

     <!-- Enhanced attributes for Audio HAL only -->
             card="<alsa card name>"
             device="<alsa device numerical id, may be empty if using alsa>"
             requirePreEnable="<0|1> if set, the audio device will be opened before calling mixer controls"
             requirePostDisable="<0|1> if set, the audio device will be closed after calling mixer controls"
             silencePrologMs="<silence in ms to be appended in the ring buffer to get rid of hw unmute delay>"
             periodSize="<period size in frames>"
             periodCount="<number of period>"
             startThreshold="<startThreshold size in frames>"
             stopThreshold="<stopThreshold size in frames>"
             silenceThreshold="<silenceThreshold size in frames>"
             availMin="<availMin size in frames>"
             dynamicChannelMapControl="<either name of numeric id of control mixer to retrieve channels supported>"
             dynamicSampleRateControl="<either name of numeric id of control mixer to retrieve rates supported>"
             dynamicFormatControl="<either name of numeric id of control mixer to retrieve formats supported>"
             supportedUseCases="<list of affinity of use case (aka input source for input, n/a for output ("|" separated)>"
             effectsSupported="<list of affinity with effects ("|" separated)>">

    <!-- End of Enhanced attributes for Audio HAL only -->

        <profile format="<format supported for this profile i.e. AUDIO_FORMAT_PCM_16_BIT>"
                 samplingRates="<list of rate supported ("|" separated)>"
                 channelMasks="<list of channel mask supported i.e. AUDIO_CHANNEL_OUT_STEREO ("|" separated)>"/>
    </mixPort>


# Rogue Parameter Example:

 Note that wrapping table is not mandatory.
 When the parameter structure is declared as an array and only one parameter value
 is received from the client (e.g for volume controls), that value will
 be duplicated before being broadcasted. This will ensure consistency with the alsa mapping.

    <rogue_parameter name="<volume_amp_media_front>" type="<string|uint32_t|double>"
                     default="<default value in the PFW parameter domain>"
                     parameter="<associated Android Parameter key>"
                     path="<Path of the rogue parameter>"
                     mapping="<Android Param 1, PFW Param 1>,<Android Param 2, PFW Param 2>,...>">
    </rogue_parameter>

# Criterion type Example:

 For each criterion, a couple of numerical, literal values must be provided to the PFW.
 The numerical part is not mandatory. If not filled by the user, a default numerical value will be
 automatically provided by audio HAL using the following logic:
   - Exclusive criterion:
          * 0 -> first literal value,
          * 1 -> second literal value,
               ...
          * N -> (N+1)th literal value.
   - Inclusive criterion:
          * 1 << 0 -> first literal value,
          * 1 << 1 -> second literal value,
               ...
          * 1 << N -> (N+1)th literal value,

    <criterion_type name="<Criterion Name>" type="<exclusive|inclusive>"
                values="<[numerical value 1:]<literal value 1>,[numerical value 2:]<literal value 2>,<literal value 3>>">
    </criterion_type>


# Criterion:

 Note that parameter and mapping are not mandatory.
 If given, it means that this criterion is associated to an Android Parameter Example:
 If not, the criterion is standalone.

    <criterion name="<Criterion Name>" type="<Criterion type name>" parameter="<associated Android Parameter key>" default="<default value of the criterion>"
               mapping="< <Android Param 1, PFW Criterion Value 1>,...>">
    </criterion>