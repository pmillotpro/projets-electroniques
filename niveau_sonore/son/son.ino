// --- Configuration des Broches ---
const int pinOUT = A0;
const int ledVerte = 2;
const int ledJaune = 3;
const int ledRouge = 4;

// --- Seuils de déclenchement (en dBSPL réels) ---
const float SEUIL_JAUNE = 70.0;
const float SEUIL_ROUGE = 80.0;
const float HYSTERESIS  = 2.0;

// --- OFFSET acoustique pour le MAX4466 (Conversion Volts -> dBSPL) ---
const float OFFSET_ACOUSTIQUE = 70.0; 

unsigned long dernierTemps = 0;
const unsigned long intervalle = 1000;

float dbLisse = 0.0;
int etatActuel = 0; // 0: Vert, 1: Jaune, 2: Rouge

float mesurerAmplitudeVolts() {
  unsigned long start = millis();
  int sMax = 0;
  int sMin = 1024;

  while (millis() - start < 30) {
    int lecture = analogRead(pinOUT);
    if (lecture < 1024) {
      if (lecture > sMax) sMax = lecture;
      if (lecture < sMin) sMin = lecture;
    }
  }

  int peakToPeak = sMax - sMin;
  return (peakToPeak * 3.3) / 1024.0; // Conversion en Volts (Alimentation 3.3V)
}

void setup() {
  Serial.begin(9600);

  pinMode(ledVerte, OUTPUT);
  pinMode(ledJaune, OUTPUT);
  pinMode(ledRouge, OUTPUT);

  // Test visuel des LEDs au démarrage
  digitalWrite(ledVerte, HIGH);
  digitalWrite(ledJaune, HIGH);
  digitalWrite(ledRouge, HIGH);
  delay(600);
  digitalWrite(ledVerte, LOW);
  digitalWrite(ledJaune, LOW);
  digitalWrite(ledRouge, LOW);
}

void loop() {
  static float echantillonsVolts[5];
  static int indexEch = 0;
  static unsigned long dernierEchTime = 0;

  if (millis() - dernierEchTime >= 150) {
    dernierEchTime = millis();
    echantillonsVolts[indexEch] = mesurerAmplitudeVolts();
    indexEch = (indexEch + 1) % 5;
  }

  if (millis() - dernierTemps >= intervalle) {
    dernierTemps = millis();

    // Filtre Médian (Anti-parasites)
    float copie[5];
    for (int i = 0; i < 5; i++) copie[i] = echantillonsVolts[i];

    for (int i = 0; i < 4; i++) {
      for (int j = i + 1; j < 5; j++) {
        if (copie[i] > copie[j]) {
          float temp = copie[i];
          copie[i] = copie[j];
          copie[j] = temp;
        }
      }
    }

    float voltsMedians = copie[2]; 

    // --- CALCUL DES dB SPL (Pression sonore) ---
    float dbInstantane = 0.0;
    if (voltsMedians > 0.0001) {
      dbInstantane = 20.0 * log10(voltsMedians) + OFFSET_ACOUSTIQUE;
    } else {
      dbInstantane = 30.0; // Plancher de silence ambiant
    }

    // Lissage
    if (dbLisse == 0.0) dbLisse = dbInstantane;
    dbLisse = (dbLisse * 0.6) + (dbInstantane * 0.4);

    // Moniteur série
    Serial.print("Niveau sonore : ");
    Serial.print(dbLisse, 1);
    Serial.println(" dB");

    // --- HYSTÉRÉSIS LEDS ---
    if (etatActuel == 0) { // Vert
      if (dbLisse >= SEUIL_JAUNE) etatActuel = 1; 
    }
    else if (etatActuel == 1) { // Jaune
      if (dbLisse >= SEUIL_ROUGE) etatActuel = 2; 
      else if (dbLisse < (SEUIL_JAUNE - HYSTERESIS)) etatActuel = 0; 
    }
    else if (etatActuel == 2) { // Rouge
      if (dbLisse < (SEUIL_ROUGE - HYSTERESIS)) etatActuel = 1; 
    }

    digitalWrite(ledVerte, etatActuel == 0 ? HIGH : LOW);
    digitalWrite(ledJaune, etatActuel == 1 ? HIGH : LOW);
    digitalWrite(ledRouge, etatActuel == 2 ? HIGH : LOW);
  }
}