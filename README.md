## Trident 2020

Note: This code no longer has a dependency on BallySternOS files (they're included here).  
Note 2: This is an older (unsupported) version of Trident 2020, kept here only because it's the last version of Trident2020 that's still small enough to run on the Nano (for Rev 1 & 2 boards).  

### To use this code
* Download the zip file (Code > Download ZIP) or clone the repository to your hard drive.  
* Unzip the Trident2020 repository and name the folder that it's in as:
  * Trident2020  
* Open the Trident2020.ino in Arduino's IDE
* At the top of Trident2020.ino and you'll see a couple of parameters in #define statements
  * if you want to use your machine's sound card, make sure this line is uncommented:  
   * #define USE_SB100  
  * if you have a Wav Trigger installed, uncomment this line 
    * #define USE_WAV_TRIGGER
    * or 
    * #define USE_WAV_TRIGGER_1p3  
  * The WAV files for the Wav Trigger board can be found here:  
    * https://drive.google.com/file/d/1kFE97fg0SsAZb8K-F-PURkWhEmwNQXq8/view?usp=sharing  
    
  
Refer to the PDF or Wiki for instructions on how to build the hardware.  
