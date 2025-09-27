let mediaRecorder;
let audioChunks = [];

function startRecording() {
    navigator.mediaDevices.getUserMedia({ audio: true }).then(stream => {
        mediaRecorder = new MediaRecorder(stream);
        mediaRecorder.start();
        audioChunks = [];

        mediaRecorder.ondataavailable = e => audioChunks.push(e.data);
        mediaRecorder.onstop = async () => {
            const blob = new Blob(audioChunks, { type: 'audio/wav' });
            const file = new File([blob], "snippet.wav", { type: 'audio/wav' });

            const formData = new FormData();
            formData.append("file", file);
            formData.append("action", "match");

            const response = await fetch("/", { method: "POST", body: formData });
            const text = await response.text();
            document.body.innerHTML = text;
        };
    });
}

function stopRecording() {
    if (mediaRecorder) mediaRecorder.stop();
}
