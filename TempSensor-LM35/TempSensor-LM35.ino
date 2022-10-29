/*Code designed by Sujay Alaspure in SA Lab */

const int sensor = A5; // Assigning analog pin A5 to variable 'sensor'
float tempc; //variable to store temperature in degree Celsius
float tempf; //variable to store temperature in Fahreinheit
float vout; //temporary variable to hold sensor reading

// 12 hour MAX runtime: 12 hours * 60 minutes * 60 seconds * 1000
//const unsigned long MAX_RUNTIME = 12 * 60 *60 * 1000;
const unsigned long MAX_RUNTIME = 43200000;

// 15 minutes MIN runtime: 15 minutes * 60 seconds * 1000
//const unsigned long MIN_RUNTIME = 900000;
const unsigned long MIN_RUNTIME = 900000;

unsigned long overallRuntime = 0;
unsigned long currentRuntime = 0;

const long CUTOFF_TEMP = 72.0L;

boolean shouldRun = false;

void setup() {
  pinMode(sensor, INPUT); // Configuring sensor pin as input
  Serial.begin(9600);
}

float randomTemp(float min, float max) {
  return random(min, max) * 1.00;
}

void loop() {
  vout = analogRead(sensor); //Reading the value from sensor
  vout = (vout * 500) / 1023;
  tempc = vout; // Storing value in Degree Celsius
  tempf = (vout * 1.8) + 32; // Converting to Fahrenheit
  //tempf = randomTemp(65, 71);

  Serial.print(tempf);
  Serial.print(" ");

  if(tempf >= CUTOFF_TEMP) {
    currentRuntime = 0;
    Serial.print("Above ");
    shouldRun = true;
  } else {
    Serial.print("Below ");
    if(shouldRun && currentRuntime <= MIN_RUNTIME) {
      Serial.print("Already running && ");
      Serial.print(currentRuntime);
      Serial.print(" <= ");
      Serial.print(MIN_RUNTIME);
      Serial.print(" ");
      shouldRun = true;
    } else {
      Serial.print("Stopped || ");
      Serial.print(currentRuntime);
      Serial.print(" > ");
      Serial.print(MIN_RUNTIME);
      Serial.print(" ");
      shouldRun = false;
    }
  }
  if(overallRuntime >= MAX_RUNTIME) {
    shouldRun = false;
  }

  if(shouldRun) {
    currentRuntime += 5000;
    overallRuntime += 5000;
    Serial.println("Running ");
  } else {
    currentRuntime = 0;
    overallRuntime = 0;
    Serial.println("Stopped ");
  }
  delay(5000);
  /*
  Serial.print("");

  Serial.print("in DegreeC=");
  Serial.print("\t");
  Serial.print(tempc);
  Serial.print(" ");
  Serial.print("in Fahrenheit=");
  Serial.print("\t");
  Serial.print(tempf);
  Serial.print(" ");
  Serial.print(currentRuntime);
  Serial.print(" ");
  Serial.print(MAX_RUNTIME);
  Serial.println();
  */
}
