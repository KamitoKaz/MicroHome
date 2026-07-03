/* ---------------------------
   Helper: safe get element
   --------------------------- */
function $(id) { return document.getElementById(id); }

const tempOverride = {// keys: element ids, values: timestamp when last changed
  doorToggle: 0,
  audioToggle: 0,
  bedroomRoomToggle: 0,
  livingRoomToggle: 0,
  kitchenLightToggle: 0,
  exhaustPower: 0,
  kitchenFanToggle: 0,
  greetButton: 0
};
const OVERRIDE_DURATION = 1000; // 1 second

async function sendCommand(endpoint, status, elementId) {
  try {
    const url = `/${endpoint}?state=${status}`;
    if (elementId && tempOverride.hasOwnProperty(elementId)) {
      tempOverride[elementId] = Date.now();
    }
    const response = await fetch(url);
    if (!response.ok) {
      console.error(`Command failed on ${endpoint}:`, await response.text());
    }
  } catch (error) {
    console.error(`Error sending command to ${endpoint}:`, error);
  }
}

/* ---------------------------
   Core elements
   --------------------------- */
const toggle = $("modeToggle");
const modeLabel = $("modeLabel");
const frame = document.querySelector(".content-box");
const navbar = document.querySelector(".navbar");
const navText = document.querySelectorAll(".nav-links a, .logo-box span, .right-side");

/* Door vars (kept your names) */
const modeToggle = $("doorToggle");
const openToggle = $("openToggle");
const openLabel = $("openLabel");
const doorCard = $("doorCard");

/* audio/exhaust */
const audioToggle = $("audioToggle");
const audioStatus = document.querySelector(".audio-card .status-value");
const audioCard = document.querySelector(".audio-card");

const exhaustAuto = $("exhaustAuto");
const exhaustPower = $("exhaustPower");
const powerLabel = $("powerLabel");
const exhaustCard = $("exhaustCard");

/* bedroom */
const bedroomAutoToggle = $("bedroom-toggle-auto");
const bedroomRoomToggle = $("bedroom-toggle-roomlight");
const bedroomRoomCard = $("bedroom-roomlight");

/* living room */
const livingAuto = $("livingroom-toggle-auto");
const livingRoomToggle = $("livingroom-toggle-roomlight");
const livingRoomCard = $("livingroom-roomlight");

/* greeting */
const greetAutoToggle = $("livingroom-toggle-greet-auto");
const greetPanel = $("livingroom-greet-panel");
const greetButton = $("livingroom-greet-btn");

/* kitchen */
const kitchenFanAuto = $("kitchen-toggle-fan-auto");
const kitchenFanToggle = $("kitchen-toggle-fan");
const kitchenFanPanel = $("kitchen-fan-panel");
const kitchenLightAuto = $("kitchen-toggle-light-auto");
const kitchenLightToggle = $("kitchen-toggle-light");
const kitchenLightPanel = $("kitchen-light-panel");

/* ---------------------------
   Dark mode / navbar toggle
   --------------------------- */
if (toggle) {

  toggle.checked = true;
  document.body.classList.add("dark");
  modeLabel.textContent = "Dark";

  toggle.addEventListener("change", () => {

    if (toggle.checked) {
      document.body.classList.add("dark");
      modeLabel.textContent = "Dark";
    } else {
      document.body.classList.remove("dark");
      modeLabel.textContent = "Light";
    }
  });
}

/* ---------------------------
   Slideshow (unchanged)
   --------------------------- */
let slideIndex = 0;
autoShowSlides();
function autoShowSlides() {
  let slides = document.getElementsByClassName("slide");
  for (let i = 0; i < slides.length; i++) slides[i].style.display = "none";
  slideIndex++;
  if (slideIndex > slides.length) slideIndex = 1;
  if (slides[slideIndex - 1]) slides[slideIndex - 1].style.display = "block";
  setTimeout(autoShowSlides, 5000);
}

const intruderCard = document.getElementById("intruder-card");
const intruderOverlay = document.getElementById("intruder-alert-overlay");
const intruderSpan = document.getElementById("attempts");

// Keep track of previous intruder value
let lastIntruderValue = null;

// Colors for 0 → 3 attempts
const intruderColors = [
  ["linear-gradient(to bottom right, #4caf50, #81c784)", "#388e3c"],   // 0 attempts: greenish
  ["linear-gradient(to bottom right, #ffcd35ff, #dfab00ff)", "#ffaf37ff"],   // 1 attempt: yellowish
  ["linear-gradient(to bottom right, #ff7423ff, #cc5200ff)", "#fd8310ff"],   // 2 attempts: orange/red
  ["linear-gradient(to bottom right, #ff1313, #ff5252)", "#7d0000"]    // 3 attempts: red
];

const intruderNotification = document.getElementById("intruder-alert");

function handleIntruder(value) {
  const val = parseInt(value);

  // Update card background/border according to attempts
  if (val === 0) {
    // Base design
    intruderCard.style.background = "var(--card-bg)";
    intruderCard.style.borderColor = "var(--card-border)";
    intruderCard.classList.remove("text-light");
    intruderNotification.style.display = "none";
  } else {
    const [bg, border] = intruderColors[Math.min(val, 3)];
    if (intruderCard) {
      intruderCard.style.background = bg;
      intruderCard.style.borderColor = border;
    }
  }
  // Show notification when attempts >= 3, hide only if back to 0
  if (val === 3) {
    intruderNotification.style.display = "block"; // stay until reset
  } else {
    intruderNotification.style.display = "none";
  }
  lastIntruderValue = val;
}



/* ---------------------------
   Door card logic
   --------------------------- */
async function manualControlDoor(status) {
  // Endpoint is setDoorState
  await sendCommand('setDoorState', status);
}
if (modeToggle && openToggle) {
  // Set Auto ON by default
  modeToggle.checked = true;
  openToggle.checked = false; // manual OFF

  // Run once to initialize
  updateOpenToggle();

  // Event listeners
  modeToggle.addEventListener("change", updateOpenToggle);
  openToggle.addEventListener("change", () => {
    tempOverride.doorToggle = Date.now();
    updateOpenToggle();
  });
}

function updateOpenToggle() {
  const card = doorCard;
  if (!card) return;
  const title = card.querySelector("h3");

  if (modeToggle.checked) {
    // Auto mode ON
    openToggle.disabled = true;
    openToggle.checked = false;

    if (openLabel) openLabel.textContent = "Close";
    card.style.background = "var(--card-bg)";
    card.style.borderColor = "var(--card-border)";
    card.classList.remove("text-light");

    manualControlDoor("AUTO");
  } else {
    // Manual mode
    openToggle.disabled = false;

    if (openToggle.checked) {
      if (openLabel) openLabel.textContent = "Open";
      card.style.background = "linear-gradient(to bottom right, #43cc25, #6be74fff, #248215)";
      card.style.borderColor = "#7ed957";
      card.classList.add("text-light");
      manualControlDoor("OPEN");
    } else {
      if (openLabel) openLabel.textContent = "Close";
      card.style.background = "var(--card-bg)";
      card.style.borderColor = "var(--card-border)";
      card.classList.remove("text-light");
      manualControlDoor("CLOSE");
    }
  }
}


/* ---------------------------
   Audio card
   --------------------------- */
async function sendAudioCommand(status) {
  await sendCommand('setAudio', status);
}

function updateAudioCard() {
  if (!audioToggle || !audioCard || !audioStatus) return;

  tempOverride.audioToggle = Date.now();

  const title = audioCard.querySelector("h3");

  if (audioToggle.checked) {
    audioStatus.textContent = "Active";
    audioCard.style.background =
      "linear-gradient(to bottom right,  #aa5fff, #9528f6, #8518a6)";
    audioCard.style.borderColor = "#9e3bdbff";
    audioStatus.style.color = "#ffffff";
    if (title) title.style.color = "#ffffff";

    sendAudioCommand("ON");
  } else {
    audioStatus.textContent = "Inactive";
    audioCard.style.background = "var(--card-bg)";
    audioCard.style.borderColor = "var(--card-border)";
    audioStatus.style.color = "var(--card-text)";
    if (title) title.style.color = "var(--card-text)";

    sendAudioCommand("OFF");
  }
}
if (audioToggle) {
  audioToggle.checked = true;      // Set toggle to ON
  updateAudioCard(); 
  audioToggle.addEventListener("change", () => {
    tempOverride.audioToggle = Date.now();
    updateAudioCard();
  });
}


/* ---------------------------
   BEDROOM logic (room light) 
   --------------------------- */
if (bedroomAutoToggle && bedroomRoomToggle && bedroomRoomCard) {

  bedroomAutoToggle.addEventListener("change", updateBedroomLight);
  bedroomRoomToggle.addEventListener("change", updateBedroomLight);

  function updateBedroomLight() {
    const autoToggle = bedroomAutoToggle;
    const roomToggle = bedroomRoomToggle;
    const roomCard = bedroomRoomCard;
    const autoState = $("bedroom-auto-state");
    const roomState = $("bedroom-room-state");
    const title = roomCard.querySelector("h4");
    const isAuto = autoToggle.checked;

    if (autoState) autoState.textContent = isAuto ? "Auto" : "Manual";
    roomCard.classList.toggle("disabled", isAuto);

    if (isAuto) {
      // ================= AUTO =================
      roomToggle.checked = false;
      if (roomState) roomState.textContent = "Off";
      roomCard.style.background = "var(--card-bg)";
      roomCard.style.borderColor = "var(--card-border)";
      roomCard.style.color = "var(--card-text)";
      if (title) title.style.color = "var(--card-text)";

      sendCommand("setBedroomLight", "AUTO");

    } else {
      // ================= MANUAL =================
      const manualIsOn = roomToggle.checked;
      if (roomState) roomState.textContent = manualIsOn ? "On" : "Off";

      if (manualIsOn) {
        roomCard.style.background = "linear-gradient(to bottom right, #46ecc8ff, #57e2d7ff, #1b5853ff)";
        roomCard.style.borderColor = "#20e2b2ff";
        roomCard.style.color = "#ffffff";
        if (title) title.style.color = "#ffffff";

        sendCommand("setBedroomLight", "ON");

      } else {
        roomCard.style.background = "var(--card-bg)";
        roomCard.style.borderColor = "var(--card-border)";
        roomCard.style.color = "var(--card-text)";
        if (title) title.style.color = "var(--card-text)";

        sendCommand("setBedroomLight", "OFF");
      }
    }
  }

  // 🔹 DEFAULT STATE (AUTO ON)
  bedroomAutoToggle.checked = true;
  bedroomRoomToggle.checked = false;

  // 🔹 Apply on load
  updateBedroomLight();
}

/* ---------------------------
   LIVING ROOM logic
   (room light)
   --------------------------- */
if (livingAuto && livingRoomToggle && livingRoomCard) {

  livingAuto.addEventListener("change", updateLivingLight);
  livingRoomToggle.addEventListener("change", updateLivingLight);

  function updateLivingLight() {
    const autoToggle = livingAuto;
    const roomToggle = livingRoomToggle;
    const roomCard = livingRoomCard;
    const autoState = $("livingroom-auto-state");
    const roomState = $("livingroom-room-state");
    const title = roomCard.querySelector("h4");
    const isAuto = autoToggle.checked;

    if (autoState) autoState.textContent = isAuto ? "Auto" : "Manual";
    roomCard.classList.toggle("disabled", isAuto);

    if (isAuto) {
      // ================= AUTO =================
      roomToggle.checked = false;
      if (roomState) roomState.textContent = "Off";
      roomCard.style.background = "var(--card-bg)";
      roomCard.style.borderColor = "var(--card-border)";
      roomCard.style.color = "var(--card-text)";
      if (title) title.style.color = "var(--card-text)";

      sendCommand("setLivingLight", "AUTO");

    } else {
      // ================= MANUAL =================
      const manualIsOn = roomToggle.checked;
      if (roomState) roomState.textContent = manualIsOn ? "On" : "Off";

      if (manualIsOn) {
        roomCard.style.background = "linear-gradient(to bottom right, #ff46a2ff, #e911dbff, #441582ff)";
        roomCard.style.borderColor = "#d80ed5ff";
        roomCard.style.color = "#ffffff";
        if (title) title.style.color = "#ffffff";

        sendCommand("setLivingLight", "ON");

      } else {
        roomCard.style.background = "var(--card-bg)";
        roomCard.style.borderColor = "var(--card-border)";
        roomCard.style.color = "var(--card-text)";
        if (title) title.style.color = "var(--card-text)";

        sendCommand("setLivingLight", "OFF");
      }
    }
  }

  // 🔹 DEFAULT STATE (AUTO ON)
  livingAuto.checked = true;
  livingRoomToggle.checked = false;

  // 🔹 Apply on load
  updateLivingLight();
}


/* ---------------------------
   GREETING panel logic
   --------------------------- */
if (greetAutoToggle && greetPanel && greetButton) {

  function updateGreetingMode() {
    const isAuto = greetAutoToggle.checked;
    const greetState = $("livingroom-greet-auto-state");

    if (greetState) greetState.textContent = isAuto ? "Auto" : "Manual";  

    if (isAuto) {
      // ================= AUTO =================
      greetButton.disabled = true;
      greetPanel.style.opacity = "0.5";
      greetPanel.style.pointerEvents = "none";
      greetPanel.style.backgroundColor = "#333";
      greetButton.style.backgroundColor = "#333";
      greetButton.style.color = "#aaa";

      sendCommand("setGreeting", "AUTO");

    } else {
      // ================= MANUAL =================
      greetButton.disabled = false;
      greetPanel.style.opacity = "1";
      greetPanel.style.pointerEvents = "auto";
      greetPanel.style.backgroundColor = "";
      greetButton.style.backgroundColor = "";
      greetButton.style.color = "";

      sendCommand("setGreeting", "MANUAL");
    }
  }

  // 🔹 DEFAULT STATE (AUTO ON)
  greetAutoToggle.checked = true;

  // 🔹 Apply state on load
  updateGreetingMode();

  // 🔹 React to user changes
  greetAutoToggle.addEventListener("change", updateGreetingMode);
}


greetButton.addEventListener("click", () => {
  sendCommand('setGreeting', "TRIGGER", "greetButton");
});



/* ---------------------------
   KITCHEN logic (fan + light)
   --------------------------- */
const kitchenAuto = $("kitchen-toggle-auto");
const kitchenToggle = $("kitchen-toggle-roomlight");
const kitchenAutoCard = $("kitchen-light-auto");
const kitchenRoomCard = $("kitchen-roomlight");

if (kitchenAuto && kitchenToggle && kitchenAutoCard && kitchenRoomCard) {

  kitchenAuto.addEventListener("change", updateKitchenLight);
  kitchenToggle.addEventListener("change", updateKitchenLight);

  function updateKitchenLight() {
    const autoState = $("kitchen-auto-state");
    const roomState = $("kitchen-room-state");
    const roomTitle = kitchenRoomCard.querySelector("h4");
    const isAuto = kitchenAuto.checked;

    // Auto / Manual text
    if (autoState) autoState.textContent = isAuto ? "Auto" : "Manual";

    // Disable room control in AUTO
    kitchenRoomCard.classList.toggle("disabled", isAuto);

    if (isAuto) {
      // ================= AUTO =================
      kitchenToggle.checked = false;
      if (roomState) roomState.textContent = "Off";

      kitchenRoomCard.style.background = "var(--card-bg)";
      kitchenRoomCard.style.borderColor = "var(--card-border)";
      kitchenRoomCard.style.color = "var(--card-text)";
      if (roomTitle) roomTitle.style.color = "var(--card-text)";

      sendCommand("setKitchenLight", "AUTO");

    } else {
      // ================= MANUAL =================
      const isOn = kitchenToggle.checked;
      if (roomState) roomState.textContent = isOn ? "On" : "Off";

      if (isOn) {
        // -------- ON --------
        kitchenRoomCard.style.background = "linear-gradient(to bottom right, #f9d55eff, #ffb300ff, #cc7402ff)";
        kitchenRoomCard.style.borderColor = "#ffb300";
        kitchenRoomCard.style.color = "#000";
        if (roomTitle) roomTitle.style.color = "#000";

        sendCommand("setKitchenLight", "ON");

      } else {
        // -------- OFF --------
        kitchenRoomCard.style.background = "var(--card-bg)";
        kitchenRoomCard.style.borderColor = "var(--card-border)";
        kitchenRoomCard.style.color = "var(--card-text)";
        if (roomTitle) roomTitle.style.color = "var(--card-text)";

        sendCommand("setKitchenLight", "OFF");
      }
    }
  }

  kitchenAuto.checked = true;     // Auto ON
  kitchenToggle.checked = false; // Manual OFF
  updateKitchenLight();
}

// Generic Auto/Manual toggle setup
let exhaustAutoState = true;   // true = AUTO
let exhaustPowerState = false; // true = ON

function setupAutoManualToggle({ autoSwitch, manualSwitch, card, stateId, titleSelector, commandName }) {
  const stateText = stateId ? $(stateId) : null;
  const title = card.querySelector(titleSelector || "h4");

  function update() {
    const isAuto = autoSwitch.checked;
    const exhaustText = $("powerLabel");
    const kitchenFanText = $("kitchen-fan-state");

    if (commandName === "setExhaust") {
      exhaustAutoState = isAuto;
      exhaustPowerState = manualSwitch.checked;
    }

    if (stateText) stateText.textContent = isAuto ? "Auto" : "Manual";

    if (isAuto) {
      manualSwitch.checked = false;
      manualSwitch.disabled = true;

      card.style.background = "var(--card-bg)";
      card.style.borderColor = "var(--card-border)";
      card.style.color = "var(--card-text)";
      if (title) title.style.color = "var(--card-text)";

      card.classList.remove("text-light", "card-on");
      sendCommand(commandName, "AUTO");

      if (commandName === "setExhaust" && exhaustText) exhaustText.textContent = "Off";
      if (commandName === "setKitchenFan" && kitchenFanText) kitchenFanText.textContent = "Off";

    } else {
      manualSwitch.disabled = false;

      if (manualSwitch.checked) {
        card.style.background = "linear-gradient(to bottom right, #11e3e9, #11b5e9, #156b82)";
        card.style.borderColor = "#56c4ffff";
        card.style.color = "#ffffff";
        if (title) title.style.color = "#ffffff";
        card.classList.add("text-light", "card-on");
        sendCommand(commandName, "ON");

        if (commandName === "setExhaust" && exhaustText) exhaustText.textContent = "On";
        if (commandName === "setKitchenFan" && kitchenFanText) kitchenFanText.textContent = "On";

      } else {
        card.style.background = "var(--card-bg)";
        card.style.borderColor = "var(--card-border)";
        card.style.color = "var(--card-text)";
        if (title) title.style.color = "var(--card-text)";
        card.classList.remove("text-light", "card-on");
        sendCommand(commandName, "OFF");

        if (commandName === "setExhaust" && exhaustText) exhaustText.textContent = "Off";
        if (commandName === "setKitchenFan" && kitchenFanText) kitchenFanText.textContent = "Off";

      }
    }
  }

  autoSwitch.addEventListener("change", update);
  manualSwitch.addEventListener("change", update);

  update(); // initialize
  return update; // return for external triggering
}

// ======== SETUP EXHAUST FAN ========
const updateExhaustFan = setupAutoManualToggle({
  autoSwitch: exhaustAuto,
  manualSwitch: exhaustPower,
  card: exhaustCard,
  stateId: "exhaust-auto-state",
  titleSelector: "h4",
  commandName: "setExhaust"
});

// ======== SETUP KITCHEN FAN ========
const updateKitchenFan = setupAutoManualToggle({
  autoSwitch: kitchenFanAuto,
  manualSwitch: kitchenFanToggle,
  card: kitchenFanPanel,
  stateId: "kitchen-fan-auto-state",
  titleSelector: "h4",
  commandName: "setKitchenFan"
});

// 🔹 DEFAULT STATE: Both fans Auto ON
if (exhaustAuto && kitchenFanAuto && exhaustPower && kitchenFanToggle) {
  // Set Auto ON
  exhaustAuto.checked = true;
  kitchenFanAuto.checked = true;

  // Set Manual power OFF
  exhaustPower.checked = false;
  kitchenFanToggle.checked = false;

  // Apply UI updates and send commands
  updateExhaustFan();
  updateKitchenFan();
}

// ======== SYNC AUTO STATES ========
let syncingFans = false; // prevent recursion
exhaustAuto.addEventListener("change", () => {
  if (syncingFans) return;
  syncingFans = true;

  if (kitchenFanAuto.checked !== exhaustAuto.checked) {
    kitchenFanAuto.checked = exhaustAuto.checked;
    updateKitchenFan();
  }
  updateExhaustFan(); // always update exhaust visuals
  syncingFans = false;
});

kitchenFanAuto.addEventListener("change", () => {
  if (syncingFans) return;
  syncingFans = true;

  if (exhaustAuto.checked !== kitchenFanAuto.checked) {
    exhaustAuto.checked = kitchenFanAuto.checked;
    updateExhaustFan();
  }
  updateKitchenFan(); // always update kitchen fan visuals
  syncingFans = false;
});

// ======== SYNC MANUAL STATES ========
let syncingFanPower = false;
exhaustPower.addEventListener("change", () => {
  if (syncingFanPower) return;
  if (exhaustAuto.checked) return; // ignore in AUTO

  syncingFanPower = true;

  if (kitchenFanToggle.checked !== exhaustPower.checked) {
    kitchenFanToggle.checked = exhaustPower.checked;
  }

  updateExhaustFan();
  updateKitchenFan();

  syncingFanPower = false;
});

kitchenFanToggle.addEventListener("change", () => {
  if (syncingFanPower) return;
  if (kitchenFanAuto.checked) return; // ignore in AUTO

  syncingFanPower = true;

  if (exhaustPower.checked !== kitchenFanToggle.checked) {
    exhaustPower.checked = kitchenFanToggle.checked;
  }

  updateKitchenFan();
  updateExhaustFan();

  syncingFanPower = false;
});

/* Kitchen light handlers (if those elements exist) */
if (kitchenLightAuto && kitchenLightToggle && kitchenLightPanel) {
  kitchenLightAuto.addEventListener("change", updateKitchenLightManual);
  kitchenLightToggle.addEventListener("change", updateKitchenLightManual);

  function updateKitchenLightManual() {
    const autoToggle = kitchenLightAuto;
    const manualSwitch = kitchenLightToggle;
    const card = kitchenLightPanel;
    const autoState = $("kitchen-light-auto-state");
    const manualState = $("kitchen-light-state");
    const title = card.querySelector("h4");
    const isAuto = autoToggle.checked;

    if (autoState) autoState.textContent = isAuto ? "Auto" : "Manual";
    card.classList.toggle("disabled", isAuto);

    if (isAuto) {
      manualSwitch.checked = false;

      if (manualState) manualState.textContent = "Off";
      card.style.background = "var(--card-bg)";
      card.style.borderColor = "var(--card-border)";
      card.style.color = "var(--card-text)";

      if (title) title.style.color = "var(--card-text)";
      sendCommand('setKitchenLight', "AUTO"); // SEND AUTO
    } else {
      const manualIsOn = manualSwitch.checked;

      if (manualIsOn) {
        card.style.background = "linear-gradient(to bottom right, #11e3e9, #11b5e9, #156b82)";
        card.style.borderColor = "#119de9";
        card.style.color = "#ffffff";

        if (title) title.style.color = "#ffffff";
        sendCommand('setKitchenLight', "ON"); // SEND ON
      } else {
        card.style.background = "var(--card-bg)";
        card.style.borderColor = "var(--card-border)";
        card.style.color = "var(--card-text)";

        if (title) title.style.color = "var(--card-text)";
        sendCommand('setKitchenLight', "OFF"); // SEND OFF
      }
    }
  }
  updateKitchenLightManual();
}


// Temporary test data for previewing design
const testData = {
  light: 75.5,
  temperature: 22.3,
  temperatureF: 72.1,
  humidity: 55.2,
  heatIndex: 24.1,
  heatIndexF: 75.4,
  sounds: 12.5,
  distance: 150.0,
  intruder: 0, // intruder attempts
  lockRemaining: 45,
  servoActive: true,
  manualControlSound: true,
  manualControlExhaust: true,
  manualControlBedroomLight: true,
  manualControlLivingLight: true,
  manualControlKitchenLight: false,
  manualControlGreeting: false,
};


async function updateStatus() {
  try {
    const response = await fetch('/status');
    if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
    const data = await response.json();

    //const data = testData; // Use this instead of fetching

    //Data to be read and updated
    document.getElementById('lit').innerText = data.light.toFixed(2);
    document.getElementById('temp').innerText = data.temperature.toFixed(2);
    document.getElementById('tempF').innerText = data.temperatureF.toFixed(2);
    document.getElementById('hum').innerText = data.humidity.toFixed(2);
    document.getElementById('hic').innerText = data.heatIndex.toFixed(2);
    document.getElementById('hif').innerText = data.heatIndexF.toFixed(2);
    document.getElementById('son').innerText = data.sounds.toFixed(2);
    document.getElementById('dis').innerText = data.distance.toFixed(2);
    document.getElementById('attempts').innerText = data.intruder;
    document.getElementById('CDlocked').innerText = data.lockRemaining;


    // React to ESP32 intruder value
    handleIntruder(data.intruder);

    //Door Status update
    if (modeToggle && openToggle) {
      // Skip update if recently changed by user
      if (!tempOverride.doorToggle || Date.now() - tempOverride.doorToggle > OVERRIDE_DURATION) {
        // If the servo is physically active (open), reflect that on the manual toggle
        openToggle.checked = data.servoActive;
        // Re-run the visual update logic
        updateOpenToggle();
      }
    }

    // Audio Status: If manualControlSound is true, the audio is ON/Active.
    if (audioToggle) {
      if (!tempOverride.audioToggle || Date.now() - tempOverride.audioToggle > OVERRIDE_DURATION) {
        audioToggle.checked = data.manualControlSound;
        updateAudioCard();
      }
    }

    // Exhaust Fan update
    if (typeof data.manualControlExhaust === "boolean") {
      if (!tempOverride.exhaustPower || Date.now() - tempOverride.exhaustPower > OVERRIDE_DURATION) {
        const espAuto = !data.manualControlExhaust;

        if (exhaustAuto.checked !== espAuto) {
          exhaustAuto.checked = espAuto;
        }
        if (kitchenFanAuto.checked !== espAuto) {
          kitchenFanAuto.checked = espAuto;
        }
        updateExhaustFan();
        updateKitchenFan();
      }
    }

    // Bedroom Light
    if (bedroomAutoToggle && bedroomRoomToggle) {
      if (!tempOverride.bedroomRoomToggle || Date.now() - tempOverride.bedroomRoomToggle > OVERRIDE_DURATION) {
        bedroomAutoToggle.checked = !data.manualControlBedroomLight;
        if (data.manualControlBedroomLight) {
          //bedroomRoomToggle.checked = data.LDR_State; // Needs a C++ flag for manual power state
        }
        updateBedroomLight();
      }
    }

    // Living Light (Logic is similar to Bedroom)
    if (livingAuto && livingRoomToggle) {
      if (!tempOverride.livingRoomToggle || Date.now() - tempOverride.livingRoomToggle > OVERRIDE_DURATION) {
        livingAuto.checked = !data.manualControlLivingLight;
        if (data.manualControlLivingLight) {
          // Placeholder: Needs a C++ flag for manual power state
        }
        updateLivingLight();
      }
    }

    // Kitchen Light
    if (kitchenLightAuto && kitchenLightToggle) {
      if (!tempOverride.kitchenLightToggle || Date.now() - tempOverride.kitchenLightToggle > OVERRIDE_DURATION) {
        kitchenLightAuto.checked = !data.manualControlKitchenLight;
        // Placeholder: Needs a C++ flag for manual power state
        updateKitchenLightManual();
      }
    }

    // Greeting Panel
    if (greetAutoToggle) {
      if (!tempOverride.greetButton || Date.now() - tempOverride.greetButton > OVERRIDE_DURATION) {
        greetAutoToggle.checked = !data.manualControlGreeting; // Auto is NOT manual
        // Manually trigger the change logic to update button disable state
        updateGreetingMode();
      }
    }

  } catch (err) {
    console.error('Error fetching data', err);
  }
}

// Update every 0.5 seconds
setInterval(updateStatus, 500);
updateStatus(); // initial call