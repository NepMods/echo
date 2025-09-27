from flask import Flask, render_template, request
import os
import subprocess
from werkzeug.utils import secure_filename

UPLOAD_FOLDER = 'uploads'
ALLOWED_EXTENSIONS = {'wav'}
DB_FILE = 'songs.db'

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

def parse_match_output(output):
    """
    Parse ./match output and return a list of dicts:
    [{'title':..., 'timestamp':..., 'score':...}, ...]
    """
    matches = []
    for line in output.splitlines():
        line = line.strip()
        if line.startswith(tuple(str(i)+")" for i in range(1,10))):
            # Example line: 1) songID=2357663286 title='song2' timestamp=40058 score=289
            try:
                parts = line.split()
                title = parts[2].split('=')[1].strip("'")
                timestamp = int(parts[3].split('=')[1])
                score = int(parts[4].split('=')[1])
                matches.append({
                    'title': title,
                    'timestamp': timestamp,
                    'score': score
                })
            except Exception as e:
                continue
    return matches

@app.route("/", methods=['GET', 'POST'])
def index():
    message = ""
    matches = None
    if request.method == 'POST':
        action = request.form.get('action')
        file = request.files.get('file')

        if file and allowed_file(file.filename):
            filename = secure_filename(file.filename)
            filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            file.save(filepath)

            if action == 'add':
                # Automatically get song name from file name
                song_name = os.path.splitext(filename)[0]  # removes '.wav'
                cmd = ['./add_to_db', DB_FILE, filepath, song_name]
                result = subprocess.run(cmd, capture_output=True, text=True)
                message = result.stdout

            elif action == 'match':
                cmd = ['./match', DB_FILE, filepath]
                result = subprocess.run(cmd, capture_output=True, text=True)
                message = result.stdout
                matches = parse_match_output(message)
        else:
            message = "Invalid file. Only .wav allowed."

    return render_template('index.html', message=message, matches=matches)

if __name__ == "__main__":
    app.run(debug=True)
