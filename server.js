const express = require('express');
const bodyParser = require('body-parser');
const fs = require('fs');
const path = require('path');
const cors = require('cors');
const app = express();

// Global variables
let totalLines = 0;
let remainingline = 0;
let interval = null;

// Middleware for parsing JSON bodies
app.use(bodyParser.json({ limit: '10mb' }));

// CORS middleware to allow all origins
app.use(cors({
    origin: '*' // Allow all origins
}));

// Static file serving (serving your HTML files and images)
app.use(express.static(path.join(__dirname, 'ESP32')));

let machineStatus = {
    progress: 0,
    remainingTime: 300,
    linesCompleted: 0,
    threadRemaining: 50,
    imageUrl: "/uploaded_image.png",
};

// Load machine state from file (if exists)
const loadMachineState = () => {
    try {
        const state = fs.readFileSync('machine_state.json', 'utf-8');
        machineStatus = JSON.parse(state);
        console.log('Machine state loaded:', machineStatus);
    } catch (err) {
        console.log('No saved machine state found, using default state.');
    }
};

// Save machine state to file
const saveMachineState = () => {
    fs.writeFileSync('machine_state.json', JSON.stringify(machineStatus, null, 2));
    console.log('Machine state saved:', machineStatus);
};

loadMachineState();  // Load saved state on server start

// Serve status data
app.get('/status', (req, res) => {
    res.json(machineStatus);
});

// Serve the image URL
app.get('/image', (req, res) => {
    res.json({ imageUrl: machineStatus.imageUrl });
});

// Serve string data status
app.get('/string-data', (req, res) => {
    const stringDataPath = 'string_data.txt';

    if (fs.existsSync(stringDataPath)) {
        const data = fs.readFileSync(stringDataPath, 'utf-8');
        const totalLines = data.split('\n').length;
        const currentLine = machineStatus.linesCompleted;

        res.json({ available: true, currentLine, totalLines });
    } else {
        res.json({ available: false });
    }
});

// Upload route to handle image and string data
app.post('/upload', (req, res) => {
    const { image, data } = req.body;

    if (!image || !data) {
        return res.status(400).send('Image and string data are required');
    }

    try {
        const imageBuffer = Buffer.from(image.split(',')[1], 'base64');
        const imagePath = path.join(__dirname, 'ESP32', 'uploaded_image.png');
        fs.writeFileSync(imagePath, imageBuffer);

        fs.writeFileSync('string_data.txt', data);

        totalLines = data.split('\n').length;
        remainingline = totalLines;
        machineStatus.imageUrl = '/uploaded_image.png';
        machineStatus.progress = 0;
        machineStatus.remainingTime = totalLines * 3;
        machineStatus.linesCompleted = 0;
        machineStatus.threadRemaining = 50;

        saveMachineState();  // Save the updated state

        res.redirect('/control.html');
    } catch (error) {
        console.error('Error saving data:', error);
        res.status(500).send('Error processing the data');
    }
});

// Helper function to run the machine
const runMachine = () => {
    return setInterval(() => {
        if (machineStatus.progress >= 100) {
            clearInterval(interval);
            machineStatus.isRunning = false;
            saveMachineState();
            return console.log('Machine completed.');
        }

        // Update progress based on lines completed and remaining lines
        const newProgress = Math.floor(((totalLines - remainingline) / totalLines) * 100);
        
        // Ensure progress doesn't exceed 100%
        machineStatus.progress = Math.min(newProgress, 100);
        remainingline -= 300;  // Update remaining lines (decrease after processing lines)
        machineStatus.remainingTime -= 10;  // Decrease remaining time as the progress increases
        machineStatus.linesCompleted += 50;  // Increase lines completed
        machineStatus.threadRemaining -= 1;  // Decrease threads remaining

        saveMachineState();  // Save the updated state
        console.log(`Progress: ${machineStatus.progress}%`);
    }, 1000);
};

// Start the machine (resume from last saved state)
app.post('/start', (req, res) => {
    if (machineStatus.progress === 100) {
        return res.status(400).send('Machine already completed.');
    }

    machineStatus.isRunning = true;
    interval = runMachine();
    res.send('Machine started from the current state.');
});

// Stop the machine (paused state)
app.post('/stop', (req, res) => {
    if (machineStatus.progress === 100) {
        return res.status(400).send('Machine already completed.');
    }

    if (interval) {
        clearInterval(interval);
        interval = null;
    }

    machineStatus.isRunning = false;
    saveMachineState();
    res.send('Machine paused at current progress.');
});

// Resume the machine from the last known state
app.post('/resume', (req, res) => {
    if (machineStatus.progress === 100) {
        return res.status(400).send('Machine already completed.');
    }

    if (machineStatus.isRunning) {
        return res.status(400).send('Machine is already running.');
    }

    machineStatus.isRunning = true;
    interval = runMachine();
    res.send('Machine resumed from last progress state.');
});

// Restart the machine (reset state)
app.post('/restart', (req, res) => {
    machineStatus = {
        progress: 0,
        remainingTime: 300,
        linesCompleted: 0,
        threadRemaining: 50,
        imageUrl: "/uploaded_image.png",
    };

    const imagePath = path.join(__dirname, 'ESP32', 'uploaded_image.png');
    if (fs.existsSync(imagePath)) {
        fs.unlinkSync(imagePath);
    }

    const stringDataPath = 'string_data.txt';
    if (fs.existsSync(stringDataPath)) {
        fs.unlinkSync(stringDataPath);
    }

    saveMachineState();
    res.send('Machine restarted and data deleted.');
});

// Start the server
const ip = '192.168.1.3';  // Or use your laptop's IP address
app.listen(5000, ip, () => {
    console.log(`Server running on ${ip}:5000`);
});