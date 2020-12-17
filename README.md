## Trident 2020

Note: This code has a dependency on BallySternOS - it won't build without those files. They're located in a repository here:
https://github.com/BallySternOS/BallySternOS
The base library is separated from this implementation so that it can be used by multiple projects without needing to be updated multiple times. For best results, always get all files (both the base library and the Trident2020 files) each time you build. Read on for basic instructions on how to build this code.


### To use this code
* Download the zip file (Code > Download ZIP) or clone the repository to your hard drive.  
* Get the BallySternOS files ( BallySternOS.* and SelfTestAndAudit.* ) from the repository here:  
 * https://github.com/BallySternOS/BallySternOS/tree/master
 * (Code > Download ZIP)
* Unzip the Trident2020 repository and name the folder that it's in as:
  * Trident2020  
* Copy BallySternOS.* and SelfTestAndAudit.* into the Trident2020 folder
* Open the Trident2020.ino in Arduino's IDE
* At the top of Trident2020.ino and you'll see a couple of parameters in #define statements
  * if you want to use your machine's sound card, make sure this line is uncommented:  
   * #define USE_SB100  
  * if you have a Wav Trigger installed, uncomment this line 
    * #define USE_WAV_TRIGGER
    * or 
    * #define USE_WAV_TRIGGER_1p3 
  
Refer to the PDF or Wiki for instructions on how to build the hardware.  
