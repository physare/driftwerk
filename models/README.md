# Hand models

`hand_detector.onnx` and `hand_landmarks_detector.onnx` are the palm detector and the
21-point hand landmark model from Google's MediaPipe Hand Landmarker
(`hand_landmarker.task`, float16), converted from TensorFlow Lite to ONNX with tf2onnx
(opset 13). Their outputs match the originals to within 0.0003.

They are licensed under the Apache License 2.0 by Google LLC:
https://github.com/google-ai-edge/mediapipe/blob/master/LICENSE

They run in ONNX Runtime 1.20.1 (MIT licence, see `third_party/onnxruntime/LICENSE`),
which the Windows build ships as `tw_onnxruntime.dll`.
