import processing.serial.*;

final int GRID_SIZE = 8; // 4 for 4x4, 8 for 8x8

Serial port;
String buff = "";
int[] depths = new int[GRID_SIZE * GRID_SIZE];
float[][] terrain = new float[GRID_SIZE][GRID_SIZE];

int scale = 700 / GRID_SIZE;
float xPress = 0.0;
float yPress = 0.0;
float xRotOffset = 0;
float xRotPos = 0;
float zRotOffset = 0;
float zRotPos = 0;
float scaleOffset = 0.5;
boolean dataReady = false;

void setup() {
  size(700, 700, P3D);

  port = new Serial(this, "/dev/cu.usbmodem101", 2000000);
  port.bufferUntil(10);

  for (int i = 0; i < depths.length; i++) depths[i] = 0;
}

void draw() {
  colorMode(HSB);
  lights();
  noStroke();
  smooth();
  background(0);

  if (!dataReady) {
    colorMode(RGB);
    fill(200, 0, 0);
    rectMode(CENTER);
    rect(width / 2, height / 2, 100, 100);
    return;
  }

  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      terrain[x][y] = depths[x + y * GRID_SIZE] / 10.0;
    }
  }

  translate(width / 2, height / 2);
  rotateX(PI / 3 - (xRotOffset + xRotPos));
  rotateZ(0 - zRotOffset - zRotPos);
  scale(scaleOffset);
  translate(-width / 2, -height / 2);

  for (int y = 0; y < GRID_SIZE - 1; y++) {
    beginShape(TRIANGLE_STRIP);
    for (int x = 0; x < GRID_SIZE; x++) {
      fill(map(terrain[x][y], 0, 400, 255, 0), 255, 255);
      vertex(x * scale, y * scale, terrain[x][y]);
      vertex(x * scale, (y + 1) * scale, terrain[x][y + 1]);
    }
    endShape();
  }
}

void serialEvent(Serial p) {
  buff = p.readString();
  if (buff == null) return;
  buff = buff.replace("\r", "").replace("\n", "").trim();
  if (buff.equals("") || buff.equals("Initializing...") || buff.equals("OK")) return;

  String[] tokens = split(buff, ',');
  if (tokens.length == GRID_SIZE * GRID_SIZE) {
    for (int i = 0; i < tokens.length; i++) {
      try {
        depths[i] = int(trim(tokens[i]));
      } catch (Exception e) {
        depths[i] = 0;
      }
    }
    dataReady = true;
  }
}

void mousePressed() { xPress = mouseX; yPress = mouseY; }

void mouseDragged() {
  xRotOffset = (mouseY - yPress) / 100.0;
  zRotOffset = (mouseX - xPress) / 100.0;
}

void mouseReleased() {
  xRotPos += xRotOffset; xRotOffset = 0;
  zRotPos += zRotOffset; zRotOffset = 0;
}

void mouseWheel(MouseEvent event) {
  scaleOffset += event.getCount() / 10.0;
}
