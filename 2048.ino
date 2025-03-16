#include "FS.h"
//This empty line is vital for compiling
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

TaskHandle_t Task0;
TaskHandle_t Task1;

// Screen dimensions and game variables
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define TILE_SIZE 50
#define GRID_SIZE 4
#define TILE_MARGIN 8

#define SAVINGS_JSON "/savings.json"

#define T_CS 5
#define T_IRQ 16

//more buttons after
#define L_UP 22
#define L_DOWN 34
#define L_LEFT 32
#define L_RIGHT 35
#define R_UP 14
#define R_DOWN 21
#define R_LEFT 33
#define R_RIGHT 27

TFT_eSPI tft = TFT_eSPI();  // Create TFT object
XPT2046_Touchscreen ts(T_CS,T_IRQ);

JsonDocument saves;
char savedJson[250]="";
/*The form of saved Json:
{ "saved_grid":[[0,0,0,0],
				[0,0,0,0],
				[0,0,0,0],
				[0,0,0,0]],
	"saved_score":0,
	"record_score":0,
  "calibrated":false
	"calData":[5000,5000,0,0]
}*/

int grid[GRID_SIZE][GRID_SIZE]={0};/*{
  {2,
  4,
  8,
  16},
    {32,
    64,
    128,
    256},
      {512,
      1024,
      2048,
      4096},
        {8192,
        0,
        0,
        0}
};*/
int colours[12]={TFT_DARKGREY,TFT_YELLOW,TFT_GREEN,TFT_OLIVE,TFT_CYAN,TFT_DARKCYAN,TFT_BLUE,TFT_NAVY,TFT_MAGENTA,TFT_PURPLE,TFT_MAROON,TFT_RED};
int score = 0;
int16_t calData[4]={5000/*X_MIN*/,5000/*Y_MIN*/,0/*X_MAX*/,0/*Y_MAX*/};
bool valid=1;
bool calibrated=0;
bool gameOver=0;
int moveDirection=-1;

void touchCalibrate(int16_t (*calibP)[4], int confirmPin){
  pinMode(confirmPin,INPUT);
  DeserializationError error=deserializeJson(saves,savedJson);
  if(!error){
    calibrated=saves["calibrated"];
    if(calibrated){
      for(int i=0;i<4;i++)calData[i]=saves["calData"][i];
    }else{
      tft.drawCentreString("Calibrating",tft.width()/2,tft.height()/2,2);
      tft.drawCentreString("XMIN:",tft.width()/2-15,tft.height()/2-15,2);
      tft.drawCentreString("YMIN:",tft.width()/2-15,tft.height()/2-30,2);
      while(digitalRead(confirmPin)){
        vTaskDelay(1);
        if(ts.tirqTouched()||ts.touched()){
          TS_Point p=ts.getPoint();
          if(p.x<*(*calibP)&&p.x!=0) *(*calibP)=p.x;
          if(p.y<*(*calibP+1)&&p.y!=0) *(*calibP+1)=p.y;  
        }
        tft.setCursor(tft.width()/2+5,tft.height()/2-10);
        tft.printf("%d ",*(*calibP));
        tft.setCursor(tft.width()/2+5,tft.height()/2-25);
        tft.printf("%d ",*(*calibP+1));
      }
      tft.setCursor(tft.width()/2+5,tft.height()/2-10);
      tft.print("    ");
      tft.setCursor(tft.width()/2+5,tft.height()/2-25);
      tft.printf("    ");
      while(!digitalRead(confirmPin))vTaskDelay(1);

      tft.drawCentreString("XMAX:",tft.width()/2-15,tft.height()/2-15,2);
      tft.drawCentreString("YMAX:",tft.width()/2-15,tft.height()/2-30,2);
      while(digitalRead(confirmPin)){
        vTaskDelay(1);
        if(ts.tirqTouched()||ts.touched()){
          TS_Point p=ts.getPoint();
          if(p.x>*(*calibP+2)/*&&p.x!=0*/) *(*calibP+2)=p.x;
          if(p.y>*(*calibP+3)/*&&p.y!=0*/) *(*calibP+3)=p.y;  
        }
        tft.setCursor(tft.width()/2+5,tft.height()/2-10);
        tft.printf("%d ",*(*calibP+2));
        tft.setCursor(tft.width()/2+5,tft.height()/2-25);
        tft.printf("%d ",*(*calibP+3));
      }
      //calibrated=1;
      while(!digitalRead(confirmPin))vTaskDelay(1);
      for(int i=0;i<4;i++)saves["calData"][i]=calData[i];
      calibrated=1;
      saves["calibrated"]=1;
      saveStorage();
    } 
  } 
}

void saveStorage(){
  serializeJson(saves,savedJson);
  File f=SPIFFS.open(SAVINGS_JSON,"w");
  if(f){
    f.write((const unsigned char *)savedJson, sizeof savedJson);
    f.close();
  }
}

void saveGrid(){
  JsonArray savedGrid=saves["saved_grid"].to<JsonArray>();
  for(int x=0;x<GRID_SIZE;x++){
    for(int y=0;y<GRID_SIZE;y++){
      savedGrid[x][y]=grid[x][y];
    }
  }
  saves["saved_score"]=score;
  saveStorage();
}

void buttonSetup(){
  pinMode(L_UP,INPUT);
  pinMode(L_DOWN,INPUT);
  pinMode(L_LEFT,INPUT);
  pinMode(L_RIGHT,INPUT);
  pinMode(R_UP,INPUT);
  pinMode(R_DOWN,INPUT);
  pinMode(R_LEFT,INPUT);
  pinMode(R_RIGHT,INPUT);
}

int getSlideDirection(){
  if(ts.tirqTouched()||ts.touched()){
    TS_Point p=ts.getPoint();
    int x1,y1,x2,y2;
    if(p.z<2000){
      x1=map(p.x,calData[0],calData[2],0,tft.width());
      y1=map(p.y,calData[1],calData[3],0,tft.height());
      while(ts.tirqTouched()||ts.touched()){
        p=ts.getPoint();
        x2=map(p.x,calData[0],calData[2],0,tft.width());
        y2=map(p.y,calData[1],calData[3],0,tft.height());
      }
    }
    if(abs(x2-x1)+abs(y2-y1)>75&&p.z<1500){
      if(abs(x2-x1)>=abs(y2-y1)){
        if(x2>=x1) return 1; //right
        else return 3; //left
      }else{
        if(y2>=y1) return 2; //down
        else return 0; //up
      }
    }
    else return -1;
  }
  else return -1;
}

// Function to start a new game
void newGame() {
  memset(grid, 0, sizeof(grid));
  valid=1;
  score = 0;
  addRandomTile();
  addRandomTile();
  drawGrid();
  tft.setCursor(10, 10);
  tft.print("Score: ");
  tft.print(score);
  valid=0;
  gameOver=0;
}

void proceedGame(){
  bool empty=1;
  for(int x=0;x<GRID_SIZE;x++){
    for(int y=0;y<GRID_SIZE;y++){
      int processingSlot=saves["saved_grid"][x][y];
      if(processingSlot>0){
        grid[x][y]=saves["saved_grid"][x][y]; 
        empty=0;
      }
    }
  }
  if(empty){
    newGame();
    saveGrid();
  }else{
    score=saves["saved_score"];
    valid=1;
    drawGrid();
    tft.setCursor(10, 10);
    tft.print("Score: ");
    tft.print(score);
    valid=0;
    gameOver=0;
  }
  
}

void updateRecord(){
  saves["record_score"]=score;
  saveStorage();
}

void resetStorage(){
  SPIFFS.remove(SAVINGS_JSON);
  ESP.restart();
}

// Function to add a random tile (2 or 4) at an empty location
void addRandomTile() {
  int emptyTiles[GRID_SIZE * GRID_SIZE][2];
  int emptyCount = 0;

  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      if (grid[x][y] == 0) {
        emptyTiles[emptyCount][0] = x;
        emptyTiles[emptyCount][1] = y;
        emptyCount++;
      }
    }
  }

  if (emptyCount > 0) {
    int r = random(emptyCount);
    int value = (random(10) < 9) ? 2 : 4;  // 90% chance of 2, 10% chance of 4
    if(valid)grid[emptyTiles[r][0]][emptyTiles[r][1]] = value;
  }else{
    gameOver=1;
    for(int x=0;x<GRID_SIZE&&gameOver;x++){
      for(int y=0;y<GRID_SIZE-1&&gameOver;y++){
        if(grid[x][y]==grid[x][y+1]){
          gameOver=0;
        }
      }
    }
    for(int y=0;y<GRID_SIZE&&gameOver;y++){
      for(int x=0;x<GRID_SIZE-1&&gameOver;x++){
        if(grid[x][y]==grid[x+1][y]){
          gameOver=0;
        }
      }
    }
  }
}

// Function to draw the game grid
void drawGrid() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Score: ");
  tft.print(score);

  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      drawTile(x, y);
    }
  }
  tft.setCursor(10, 290);
  tft.print("Record: ");
  tft.print((int)saves["record_score"]);
}

// Function to draw a single tile
void drawTile(int x, int y) {
  int value = grid[x][y];
  int xpos = x * (TILE_SIZE + TILE_MARGIN) + TILE_MARGIN;
  int ypos = y * (TILE_SIZE + TILE_MARGIN) + TILE_MARGIN + 40;  // Adjust to leave space for score

  if (value > 0) {
    int colourSeq=log(value)/log(2)-1;
    tft.fillRect(xpos, ypos, TILE_SIZE, TILE_SIZE, colours[colourSeq%12]);
    tft.setTextColor(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(value), xpos + TILE_SIZE / 2, ypos + TILE_SIZE / 2);
  } else {
    tft.fillRect(xpos, ypos, TILE_SIZE, TILE_SIZE, TFT_LIGHTGREY);
  }
}

// Function to move tiles in the direction of the swipe
void moveTiles(int direction) {
  // Implement the merging and moving logic based on the direction
  // 0 = up, 1 = right, 2 = down, 3 = left
  // For each direction, slide tiles and merge similar numbers.
  int updating[4]={0,0,0,0};
  valid=0;
  switch(direction){
    case 0:
      for(int x=0;x<GRID_SIZE;x++){
        int updateSeq=0;
        for(int y=0;y<GRID_SIZE;y++){
          if(grid[x][y]>0){
            updating[updateSeq]=grid[x][y];
            updateSeq++;
          }
        }
        //Serial.printf("updating:%d%d%d%d\nGridx:%d%d%d%d\n",updating[0],updating[1],updating[2],updating[3],grid[x][0],grid[x][1],grid[x][2],grid[x][3]);
        if(updating[0]==updating[1]&&updating[2]==updating[3]&&updating[0]>0&&updating[1]>0&&updating[2]>0&&updating[3]>0){
          score+=updating[0]+updating[1];
          updating[0]=updating[0]+updating[1];
          score+=updating[2]+updating[3];
          updating[1]=updating[2]+updating[3];
          updating[2]=0;
          updating[3]=0;
        }else{
          for(int i=1;i<GRID_SIZE;i++){
            if(updating[i-1]==updating[i]||updating[i-1]==0){
              score+=updating[i-1]+updating[i];
              updating[i-1]=updating[i-1]+updating[i];
              updating[i]=0;
            }
          }
        }
        for(int i=0;i<4;i++){
          if(grid[x][i]!=updating[i]){
            grid[x][i]=updating[i];
            valid=1;
          }
          updating[i]=0;
        }
        updateSeq=0;
      }
      break;
    case 1:
      for(int y=0;y<GRID_SIZE;y++){
        int updateSeq=3;
        for(int x=GRID_SIZE-1;x>=0;x--){
          if(grid[x][y]>0){
            updating[updateSeq]=grid[x][y];
            updateSeq--;
          }
        }
        //Serial.printf("updating:%d%d%d%d\nGridx:%d%d%d%d\n",updating[0],updating[1],updating[2],updating[3],grid[x][0],grid[x][1],grid[x][2],grid[x][3]);
        if(updating[0]==updating[1]&&updating[2]==updating[3]&&updating[0]>0&&updating[1]>0&&updating[2]>0&&updating[3]>0){
          score+=updating[2]+updating[3];
          updating[3]=updating[2]+updating[3];
          score+=updating[0]+updating[1];
          updating[2]=updating[0]+updating[1];
          updating[0]=0;
          updating[1]=0;
        }else{
          for(int i=(GRID_SIZE-1)-1;i>=0;i--){
            if(updating[i+1]==updating[i]||updating[i+1]==0){
              score+=updating[i+1]+updating[i];
              updating[i+1]=updating[i+1]+updating[i];
              updating[i]=0;
            }
          }
        }
       for(int i=3;i>=0;i--){
          if(grid[i][y]!=updating[i]){
            grid[i][y]=updating[i];
            valid=1;
          }
          updating[i]=0;
        }
        updateSeq=0;
      }
      break;
    case 2:
      for(int x=0;x<GRID_SIZE;x++){
        int updateSeq=3;
        for(int y=GRID_SIZE-1;y>=0;y--){
          if(grid[x][y]>0){
            updating[updateSeq]=grid[x][y];
            updateSeq--;
          }
        }
        //Serial.printf("updating:%d%d%d%d\nGridx:%d%d%d%d\n",updating[0],updating[1],updating[2],updating[3],grid[x][0],grid[x][1],grid[x][2],grid[x][3]);
        if(updating[0]==updating[1]&&updating[2]==updating[3]&&updating[0]>0&&updating[1]>0&&updating[2]>0&&updating[3]>0){
          score+=updating[2]+updating[3];
          updating[3]=updating[2]+updating[3];
          score+=updating[0]+updating[1];
          updating[2]=updating[0]+updating[1];
          updating[0]=0;
          updating[1]=0;
        }else{
          for(int i=(GRID_SIZE-1)-1;i>=0;i--){
            if(updating[i+1]==updating[i]||updating[i+1]==0){
              score+=updating[i+1]+updating[i];
              updating[i+1]=updating[i+1]+updating[i];
              updating[i]=0;
            }
          }
        }
       for(int i=3;i>=0;i--){
          if(grid[x][i]!=updating[i]){
            grid[x][i]=updating[i];
            valid=1;
          }
          updating[i]=0;
        }
        updateSeq=0;
      }
      break;
    case 3:
      for(int y=0;y<GRID_SIZE;y++){
        int updateSeq=0;
        for(int x=0;x<GRID_SIZE;x++){
          if(grid[x][y]>0){
            updating[updateSeq]=grid[x][y];
            updateSeq++;
          }
        }
        //Serial.printf("updating:%d%d%d%d\nGridx:%d%d%d%d\n",updating[0],updating[1],updating[2],updating[3],grid[x][0],grid[x][1],grid[x][2],grid[x][3]);
        if(updating[0]==updating[1]&&updating[2]==updating[3]&&updating[0]>0&&updating[1]>0&&updating[2]>0&&updating[3]>0){
          score+=updating[0]+updating[1];
          updating[0]=updating[0]+updating[1];
          score+=updating[2]+updating[3];
          updating[1]=updating[2]+updating[3];
          updating[2]=0;
          updating[3]=0;
        }else{
          for(int i=1;i<GRID_SIZE;i++){
            if(updating[i-1]==updating[i]||updating[i-1]==0){
              score+=updating[i-1]+updating[i];
              updating[i-1]=updating[i-1]+updating[i];
              updating[i]=0;
            }
          }
        }
        for(int i=0;i<4;i++){
          if(grid[i][y]!=updating[i]){
            grid[i][y]=updating[i];
            valid=1;
          }
          updating[i]=0;
        }
        updateSeq=0;
      }
      break;
    default:
      break;
  }
}

/*/ Function to check if the game is over (no more moves)
bool checkGameOver() {
  // Implement logic to check if no moves are possible
  // Return true if game is over, false otherwise
  return false;  // Placeholder
}*/

// Function to draw "Game Over" message
void drawGameOver() {
  tft.fillRect(0, SCREEN_HEIGHT / 2 - 20, SCREEN_WIDTH, 40, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Game Over", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
}

void InitStorage(){
  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }
  if(SPIFFS.exists(SAVINGS_JSON)){
    File f=SPIFFS.open(SAVINGS_JSON,"r");
    if(f&&f.size()==sizeof savedJson){
      f.readBytes((char *)savedJson, f.size());
      Serial.println(savedJson);
      f.close();
    }else{
      f.close();
      Serial.println("Savings are broken");
      SPIFFS.remove(SAVINGS_JSON);
      ESP.restart();
    }
  }else{
    strcpy(savedJson,"{\"saved_grid\":[[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]],\"saved_score\":0,\"record_score\":0,\"calibrated\":0,\"calData\":[5000,5000,0,0]}");
    File f = SPIFFS.open(SAVINGS_JSON, "w");
    if (f) {
      f.write((const unsigned char *)savedJson, sizeof savedJson);
      f.close();
      ESP.restart();
    }
  }
}

void setup() {
  Serial.begin(115200);
  xTaskCreatePinnedToCore(
                    Task0code,   /* Task function. */
                    "Task0",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 
  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
  delay(500); 
}

void Task0code(void * pvParameters){
  tft.init();
  tft.setRotation(0);  // Adjust rotation as needed
  ts.setRotation(2-tft.getRotation()%2);
  tft.fillScreen(TFT_BLACK);
  InitStorage();
  touchCalibrate(&calData,L_UP);
  tft.fillScreen(TFT_WHITE);
  randomSeed(analogRead(13)); // Seed for random tile placement
  proceedGame();
  for(;;){
    vTaskDelay(1);
    if(moveDirection>=0){
      moveTiles(moveDirection);
      moveDirection=-1;
      addRandomTile();
      drawGrid();
      if(gameOver){
        drawGameOver();
        if(score>saves["record_score"])updateRecord();
      }
    }
  }
}

void Task1code(void * pvParameters){
  buttonSetup();
  ts.begin();
  bool pressed=0;
  for(;;){
    vTaskDelay(1);
    if(calibrated){
      if(!gameOver){
        moveDirection=getSlideDirection();
        if(!digitalRead(R_UP)&&!pressed){
          moveDirection=0;
          pressed=1;
        }else if(!digitalRead(R_DOWN)&&!pressed){
          moveDirection=2;
          pressed=1;
        }else if(!digitalRead(R_LEFT)&&!pressed){
          moveDirection=3;
          pressed=1;
        }else if(!digitalRead(R_RIGHT)&&!pressed){
          moveDirection=1;
          pressed=1;
        }else if(!digitalRead(L_LEFT)&&!pressed){
          pressed=1;
          tft.fillScreen(TFT_GREEN);
          saveGrid();
          drawGrid();
        }else if(!digitalRead(L_DOWN)&&!pressed){
          resetStorage();
        } 
      }
      if(!digitalRead(L_UP)&&!pressed){
        newGame();
        pressed=1;
      }
    }
    if(digitalRead(R_UP)&&digitalRead(R_DOWN)&&digitalRead(R_LEFT)&&digitalRead(R_RIGHT)&&digitalRead(L_DOWN)&&digitalRead(L_UP)&&digitalRead(L_LEFT))pressed=0;
  }
}

void loop() {
  
}
