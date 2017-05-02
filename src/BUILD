cc_binary(
  name = "audio_player",
  srcs = [
    "audio_player/audio_player.cc",
  ],
  deps = [
    "//third_party/ffmpeg:avcodec",
    "//third_party/ffmpeg:avformat",
    "//third_party/ffmpeg:swresample",
  ],
)

cc_binary(
  name = "to_aac",
  srcs = [
    "to_aac/to_aac.cc",
  ],
  deps = [
    "//third_party/ffmpeg:avcodec",
    "//third_party/ffmpeg:avformat",
    "//third_party/ffmpeg:swresample",
  ],
)
