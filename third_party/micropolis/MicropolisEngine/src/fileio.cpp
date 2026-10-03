/* fileio.cpp
 *
 * Micropolis, Unix Version.  This game was released for the Unix platform
 * in or about 1990 and has been modified for inclusion in the One Laptop
 * Per Child program.  Copyright (C) 1989 - 2007 Electronic Arts Inc.  If
 * you need assistance with this program, you may contact:
 *   http://wiki.laptop.org/go/Micropolis  or email  micropolis@laptop.org.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  You should have received a
 * copy of the GNU General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 *             ADDITIONAL TERMS per GNU GPL Section 7
 *
 * No trademark or publicity rights are granted.  This license does NOT
 * give you any right, title or interest in the trademark SimCity or any
 * other Electronic Arts trademark.  You may not distribute any
 * modification of this program using the trademark SimCity or claim any
 * affliation or association with Electronic Arts Inc. or its employees.
 *
 * Any propagation or conveyance of this program must include this
 * copyright notice and these terms.
 *
 * If you convey this program (or any modifications of it) and assume
 * contractual liability for the program to recipients of it, you agree
 * to indemnify Electronic Arts for any liability that those contractual
 * assumptions impose on Electronic Arts.
 *
 * You may not misrepresent the origins of this program; modified
 * versions of the program must be marked as such and not identified as
 * the original program.
 *
 * This disclaimer supplements the one included in the General Public
 * License.  TO THE FULLEST EXTENT PERMISSIBLE UNDER APPLICABLE LAW, THIS
 * PROGRAM IS PROVIDED TO YOU "AS IS," WITH ALL FAULTS, WITHOUT WARRANTY
 * OF ANY KIND, AND YOUR USE IS AT YOUR SOLE RISK.  THE ENTIRE RISK OF
 * SATISFACTORY QUALITY AND PERFORMANCE RESIDES WITH YOU.  ELECTRONIC ARTS
 * DISCLAIMS ANY AND ALL EXPRESS, IMPLIED OR STATUTORY WARRANTIES,
 * INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY, SATISFACTORY QUALITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS, AND WARRANTIES (IF ANY) ARISING FROM A COURSE OF DEALING,
 * USAGE, OR TRADE PRACTICE.  ELECTRONIC ARTS DOES NOT WARRANT AGAINST
 * INTERFERENCE WITH YOUR ENJOYMENT OF THE PROGRAM; THAT THE PROGRAM WILL
 * MEET YOUR REQUIREMENTS; THAT OPERATION OF THE PROGRAM WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT THE PROGRAM WILL BE COMPATIBLE
 * WITH THIRD PARTY SOFTWARE OR THAT ANY ERRORS IN THE PROGRAM WILL BE
 * CORRECTED.  NO ORAL OR WRITTEN ADVICE PROVIDED BY ELECTRONIC ARTS OR
 * ANY AUTHORIZED REPRESENTATIVE SHALL CREATE A WARRANTY.  SOME
 * JURISDICTIONS DO NOT ALLOW THE EXCLUSION OF OR LIMITATIONS ON IMPLIED
 * WARRANTIES OR THE LIMITATIONS ON THE APPLICABLE STATUTORY RIGHTS OF A
 * CONSUMER, SO SOME OR ALL OF THE ABOVE EXCLUSIONS AND LIMITATIONS MAY
 * NOT APPLY TO YOU.
 */

/** @file fileio.cpp */

////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "micropolis.h"


////////////////////////////////////////////////////////////////////////


#ifdef IS_INTEL

/**
 * Convert an array of short values between MAC and Intel endian formats.
 * @param buf Array with shorts.
 * @param len Number of short values in the array.
 */
#define SWAP_SHORTS(buf, len)        swap_shorts(buf, len)

/**
 * Convert an array of long values between MAC and Intel endian formats.
 * @param buf Array with longs.
 * @param len Number of long values in the array.
 */
#define HALF_SWAP_LONGS(buf, len)    half_swap_longs(buf, len)

/**
 * Swap upper and lower byte of all shorts in the array.
 * @param buf Array with shorts.
 * @param len Number of short values in the array.
 */
static void swap_shorts(short *buf, int len)
{
    int i;

    /* Flip bytes in each short! */
    for (i = 0; i < len; i++) {
        *buf = ((*buf & 0xFF) <<8) | ((*buf &0xFF00) >>8);
        buf++;
    }
}


/**
 * Swap upper and lower words of all longs in the array.
 * @param buf Array with longs.
 * @param len Number of long values in the array.
 */
static void __attribute__((unused)) half_swap_longs(long *buf, int len)
{
    int i;

    /* Flip bytes in each long! */
    for (i = 0; i < len; i++) {
        long l = *buf;
        *buf =
            ((l & 0x0000ffff) << 16) |
            ((l & 0xffff0000) >> 16);
        buf++;
    }
}


#else


/**
 * Convert an array of short values between MAC and MAC endian formats.
 * @param buf Array with shorts.
 * @param len Number of short values in the array.
 * @note This version does not change anything since the data is already in the
 *       correct format.
 */
#define SWAP_SHORTS(buf, len)

/**
 * Convert an array of long values between MAC and MAC endian formats.
 * @param buf Array with longs.
 * @param len Number of long values in the array.
 * @note This version does not change anything since the data is already in the
 *       correct format.
 */
#define HALF_SWAP_LONGS(buf, len)


#endif

/**
 * Load an array of short values from file to memory.
 *
 * Convert to the correct processor architecture, if necessary.
 * @param buf Buffer to put the loaded short values in.
 * @param len Number of short values to load.
 * @param f   File handle of the file to load from.
 * @return Load was succesfull.
 */
static bool load_short(short *buf, int len, FILE *f)
{
    size_t result = fread(buf, sizeof(short), len, f);

    if ((int)result != len) {
         return false;
    }

    SWAP_SHORTS(buf, len);        /* to intel */

    return true;
}


/**
 * Save an array of short values from memory to file.
 *
 * Convert to the correct endianness first, if necessary.
 * @param buf Buffer containing the short values to save.
 * @param len Number of short values to save.
 * @param f   File handle of the file to save to.
 * @return Save was succesfull.
 */
static bool save_short(short *buf, int len, FILE *f)
{
    SWAP_SHORTS(buf, len);        /* to MAC */

    // A short write used to return with the buffer still in Mac order,
    // so the live map and history stayed scrambled after a full disk.
    const bool ok = (int)fwrite(buf, sizeof(short), len, f) == len;

    SWAP_SHORTS(buf, len);        /* back to intel */

    return ok;
}

/**
 * Classic .cty files store a 32-bit value in exactly two shorts, with the
 * 16-bit halves swapped (the Mac long the original saver wrote). Quad is
 * 8 bytes on this build, so writing one also covered the next two shorts.
 */
static void put_mac_long(short *slot, Quad value)
{
    const unsigned int bits = (unsigned int)(int)value;
    const unsigned int swapped = ((bits & 0xffffu) << 16) | (bits >> 16);
    slot[0] = (short)(swapped & 0xffffu);
    slot[1] = (short)((swapped >> 16) & 0xffffu);
}

static Quad get_mac_long(const short *slot)
{
    const unsigned int low = (unsigned short)slot[0];
    const unsigned int high = (unsigned short)slot[1];
    const unsigned int swapped = low | (high << 16);
    const unsigned int bits = ((swapped & 0xffffu) << 16) | (swapped >> 16);
    return (Quad)(int)bits;
}

static const char kCityNameMagic[4] = {'L', 'C', 'N', '1'};

static bool write_city_name(FILE *f, const std::string &name)
{
    unsigned int len = (unsigned int)name.size();
    if (len > 200u) {
        len = 200u;
    }
    if (fwrite(kCityNameMagic, 1, 4, f) != 4) {
        return false;
    }
    const unsigned char le[2] = {
        (unsigned char)(len & 0xffu),
        (unsigned char)((len >> 8) & 0xffu),
    };
    if (fwrite(le, 1, 2, f) != 2) {
        return false;
    }
    if (len > 0 && fwrite(name.data(), 1, len, f) != len) {
        return false;
    }
    return true;
}

static bool read_city_name(FILE *f, std::string &name)
{
    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        return false;
    }
    if (magic[0] != kCityNameMagic[0] || magic[1] != kCityNameMagic[1] ||
        magic[2] != kCityNameMagic[2] || magic[3] != kCityNameMagic[3]) {
        return false;
    }
    unsigned char le[2];
    if (fread(le, 1, 2, f) != 2) {
        return false;
    }
    const unsigned int len = (unsigned int)le[0] | ((unsigned int)le[1] << 8);
    if (len > 200u) {
        return false;
    }
    name.assign(len, '\0');
    if (len > 0 && fread(&name[0], 1, len, f) != len) {
        return false;
    }
    return true;
}

/**
 * Load a city file from a given filename and (optionally) directory.
 * @param filename Name of the file to load.
 * @param dir      If not \c NULL, name of the directory containing the file.
 * @return Load was succesfull.
 */
bool Micropolis::loadFileDir(const char *filename, const char *dir)
{
    char *path = NULL;
    FILE *f;
    Quad size;

    // If needed, construct a path to the file.
    if (dir != NULL) {
        path = (char *)malloc(strlen(dir) + 1 + strlen(filename) + 1);
        sprintf(path, "%s/%s", dir, filename);
        filename = path;
    }

    // Open the file.
    f = fopen(filename, "rb");

    // Before checking whether open() succeeded, first drop the path.
    if (path != NULL) {
        free(path);
    }

    // open() failed; report failure.
    if (f == NULL) {
        return false;
    }

    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    fseek(f, 0L, SEEK_SET);

    // 27120 is the classic payload. Newer saves append a city-name trailer.
    // A short file is not a city; extra bytes that are not a name are ignored.
    cityNameStored = false;
    cityNameStoredText.clear();

    bool result =
      (size >= 27120) &&
      load_short(resHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(comHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(indHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(crimeHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(pollutionHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(moneyHist, HISTORY_LENGTH / sizeof(short), f) &&
      load_short(miscHist, MISC_HISTORY_LENGTH / sizeof(short), f) &&
      load_short(((short *)&map[0][0]), WORLD_W * WORLD_H, f);

    if (result && size > 27120) {
        std::string stored;
        if (read_city_name(f, stored)) {
            cityNameStored = true;
            cityNameStoredText = stored;
        }
    }

    fclose(f);

    return result;
}

/**
 * Load a file, and initialize the game variables.
 * @param filename Name of the file to load.
 * @return Load was succesfull.
 */
bool Micropolis::loadFile(const char *filename)
{
    long n;

    if (!loadFileDir(filename, NULL)) {
        return false;
    }

    /* total funds is a 32-bit value in two shorts at miscHist[50]. */
    n = get_mac_long(miscHist + 50);
    setFunds(n);

    /* cityTime shares that width. An 8-byte Quad also covered crimeRamp
       and pollutionRamp in miscHist[10] and miscHist[11]. */
    n = get_mac_long(miscHist + 8);
    cityTime = n;

    setAutoBulldoze(miscHist[52] != 0);   // flag for autoBulldoze
    setAutoBudget(miscHist[53] != 0);     // flag for autoBudget
    setAutoGoto(miscHist[54] != 0);       // flag for auto-goto
    setEnableSound(miscHist[55] != 0);    // flag for the sound on/off
    setCityTax(miscHist[56]);
    setSpeed(miscHist[57]);
    changeCensus();
    mustUpdateOptions = true;

    /* yayaya */

    n = get_mac_long(miscHist + 58);
    policePercent = ((float)n) / ((float)65536);

    n = get_mac_long(miscHist + 60);
    firePercent = (float)n / (float)65536.0;

    n = get_mac_long(miscHist + 62);
    roadPercent = (float)n / (float)65536.0;

    cityTime = max((Quad)0, cityTime);

    // If the tax is nonsensical, set it to a reasonable value.
    if (cityTax > 20 || cityTax < 0) {
        setCityTax(7);
    }

    // If the speed is nonsensical, set it to a reasonable value.
    if (simSpeed < 0 || simSpeed > 3) {
        setSpeed(3);
    }

    setSpeed(simSpeed);
    setPasses(1);

    // initFundingLevel() forces 100% and simLoadInit() maxes the service
    // effects. Keep the percents that were just read so the next tax year
    // bills them and the effects match.
    const float savedRoadPercent = roadPercent;
    const float savedPolicePercent = policePercent;
    const float savedFirePercent = firePercent;

    initFundingLevel();

    // Set the scenario id to 0.
    initWillStuff();
    scenario = SC_NONE;
    initSimLoad = 1;
    doInitialEval = false;
    doSimInit();

    roadPercent = savedRoadPercent;
    policePercent = savedPolicePercent;
    firePercent = savedFirePercent;
    if (roadPercent < 0.0f) {
        roadPercent = 0.0f;
    } else if (roadPercent > 1.0f) {
        roadPercent = 1.0f;
    }
    if (policePercent < 0.0f) {
        policePercent = 0.0f;
    } else if (policePercent > 1.0f) {
        policePercent = 1.0f;
    }
    if (firePercent < 0.0f) {
        firePercent = 0.0f;
    } else if (firePercent > 1.0f) {
        firePercent = 1.0f;
    }
    roadEffect = (Quad)(roadPercent * (float)MAX_ROAD_EFFECT);
    policeEffect = (Quad)(policePercent * (float)MAX_POLICE_STATION_EFFECT);
    fireEffect = (Quad)(firePercent * (float)MAX_FIRE_STATION_EFFECT);

    invalidateMaps();

    return true;
}


/**
 * Save a game to disk.
 * @param filename Name of the file to use for storing the game.
 * @return The game was saved successfully.
 */
bool Micropolis::saveFile(const char *filename)
{
    FILE *f;

    if ((f = fopen(filename, "wb")) == NULL) {
        /// @todo Report error saving file.
        return false;
    }

    /* total funds is a long.....    miscHist is array of ints */
    /* total funds is bien put in the 50th & 51th word of miscHist */
    /* find the address, cast the ptr to a longPtr, take contents */

    put_mac_long(miscHist + 50, totalFunds);

    // Two shorts only. miscHist[10] and [11] are crimeRamp and pollutionRamp.
    put_mac_long(miscHist + 8, cityTime);

    miscHist[52] = autoBulldoze;   // flag for autoBulldoze
    miscHist[53] = autoBudget;     // flag for autoBudget
    miscHist[54] = autoGoto;       // flag for auto-goto
    miscHist[55] = enableSound;    // flag for the sound on/off
    miscHist[57] = simSpeed;
    miscHist[56] = cityTax;        /* post release */

    /* yayaya */

    put_mac_long(miscHist + 58, (Quad)(int)(policePercent * 65536));
    put_mac_long(miscHist + 60, (Quad)(int)(firePercent * 65536));
    put_mac_long(miscHist + 62, (Quad)(int)(roadPercent * 65536));

    bool result =
        save_short(resHist, HISTORY_LENGTH / 2, f) &&
        save_short(comHist, HISTORY_LENGTH / 2, f) &&
        save_short(indHist, HISTORY_LENGTH / 2, f) &&
        save_short(crimeHist, HISTORY_LENGTH / 2, f) &&
        save_short(pollutionHist, HISTORY_LENGTH / 2, f) &&
        save_short(moneyHist, HISTORY_LENGTH / 2, f) &&
        save_short(miscHist, MISC_HISTORY_LENGTH / 2, f) &&
        save_short(((short *)&map[0][0]), WORLD_W * WORLD_H, f) &&
        write_city_name(f, cityName);

    fclose(f);

    return result;
}


/**
 * Load a scenario.
 * @param s Scenario to load.
 * @note \a s cannot be \c SC_NONE.
 */
void Micropolis::loadScenario(Scenario s)
{
    const char *name = NULL;
    const char *fname = NULL;

    cityFileName = "";

    setGameLevel(LEVEL_EASY);

    if (s < SC_DULLSVILLE || s > SC_RIO) {
        s = SC_DULLSVILLE;
    }

    switch (s) {
        case SC_DULLSVILLE:
            name = "Dullsville";
            fname = "snro.111";
            scenario = SC_DULLSVILLE;
            cityTime = ((1900 - 1900) * 48) + 2;
            setFunds(5000);
            break;
        case SC_SAN_FRANCISCO:
            name = "San Francisco";
            fname = "snro.222";
            scenario = SC_SAN_FRANCISCO;
            cityTime = ((1906 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_HAMBURG:
            name = "Hamburg";
            fname = "snro.333";
            scenario = SC_HAMBURG;
            cityTime = ((1944 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_BERN:
            name = "Bern";
            fname = "snro.444";
            scenario = SC_BERN;
            cityTime = ((1965 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_TOKYO:
            name = "Tokyo";
            fname = "snro.555";
            scenario = SC_TOKYO;
            cityTime = ((1957 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_DETROIT:
            name = "Detroit";
            fname = "snro.666";
            scenario = SC_DETROIT;
            cityTime = ((1972 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_BOSTON:
            name = "Boston";
            fname = "snro.777";
            scenario = SC_BOSTON;
            cityTime = ((2010 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        case SC_RIO:
            name = "Rio de Janeiro";
            fname = "snro.888";
            scenario = SC_RIO;
            cityTime = ((2047 - 1900) * 48) + 2;
            setFunds(20000);
            break;
        default:
            NOT_REACHED();
            break;
    }

    setCleanCityName(name);
    setSpeed(3);
    setCityTax(7);

    loadFileDir(
        fname,
        resourceDir.c_str());

    initWillStuff();
    initFundingLevel();
    updateFunds();
    invalidateMaps();
    initSimLoad = 1;
    doInitialEval = false;
    doSimInit();
    didLoadScenario();
}


/** Report to the front-end that the scenario was loaded. */
void Micropolis::didLoadScenario()
{
    callback("didLoadScenario", "");
}

/**
 * Try to load a new game from disk.
 * @param filename Name of the file to load.
 * @return Game was loaded successfully.
 * @todo In what state is the game left when loading fails?
 * @todo String normalization code is duplicated in Micropolis::saveCityAs().
 *       Extract to a sub-function.
 * @bug Function fails if \c lastDot<lastSlash (ie with \c "x.y/bla" )
 */
bool Micropolis::loadCity(const char *filename)
{
    if (loadFile(filename)) {

        cityFileName = filename;

        const std::string::size_type lastSlash = cityFileName.find_last_of('/');
        const std::string::size_type pos = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;

        const std::string::size_type lastDot = cityFileName.find_last_of('.');
        const std::string::size_type last =
            (lastDot == std::string::npos || lastDot < pos) ? cityFileName.length() : lastDot;

        // Old saves have no name field. The filename stem is the fallback.
        // Newer saves store the real name, spaces included, and must not
        // be run through setCityName() (that turns non-alphanumerics into '_').
        if (cityNameStored) {
            setCleanCityName(cityNameStoredText);
        } else {
            std::string newCityName = cityFileName.substr(pos, last - pos);
            setCleanCityName(newCityName);
        }

        didLoadCity();

        return true;

    } else {

        didntLoadCity((filename && *filename) ? filename : "(null)");

        return false;

    }
}

/** Report to the frontend that the game was successfully loaded. */
void Micropolis::didLoadCity()
{
    callback("didLoadCity", "");
}


/**
 * Report to the frontend that the game failed to load.
 * @param msg File that attempted to load
 */
void Micropolis::didntLoadCity(const char *msg)
{
    callback("didntLoadCity", "s", msg);
}


/**
 * Try to save the game.
 * @todo This is a no-op if the Micropolis::cityFileName is empty.
 *       In that case, we should probably warn the user about the failure.
 */
void Micropolis::saveCity()
{
    if (cityFileName.length() > 0) {

        doSaveCityAs();

    } else {
        if (saveFile(cityFileName.c_str())) {

            didSaveCity();

        } else {

            didntSaveCity(cityFileName.c_str());

        }
    }
}


/** Report to the frontend that the city is being saved. */
void Micropolis::doSaveCityAs()
{
    callback("saveCityAs", "");
}


/** Report to the frontend that the city was saved successfully. */
void Micropolis::didSaveCity()
{
    callback("didSaveCity", "");
}


/**
 * Report to the frontend that the city could not be saved.
 * @param msg Name of the file used
 */
void Micropolis::didntSaveCity(const char *msg)
{
    callback("didntSaveCity", "s", msg);
}


/**
 * Save the city under a new name (?)
 * @param filename Name of the file to use for storing the game.
 * @todo String normalization code is duplicated in Micropolis::loadCity().
 *       Extract to a sub-function.
 * @bug Function fails if \c lastDot<lastSlash (ie with \c "x.y/bla" )
 */
void Micropolis::saveCityAs(const char *filename)
{
    cityFileName = filename;

    if (saveFile(cityFileName.c_str())) {

        // The file stem is not the city name. Rename City, and names with
        // spaces, have to survive Save and Save As.
        didSaveCity();

    } else {

        didntSaveCity(cityFileName.c_str());

    }
}


////////////////////////////////////////////////////////////////////////
