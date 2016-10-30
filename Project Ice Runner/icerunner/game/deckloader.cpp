#include <lua.hpp>
#include "deckloader.h"
#include "mapkit.hpp"

namespace ice
{

namespace game
{

bool DeckLoader::Init()
{
    pLuaState_ = luaL_newstate();
    if (!pLuaState_) return false;
    if(!ExportConfigInterface(pLuaState_)) return false;
    return true;
}

//! Maybe also have a shutdown?
DeckLoader::~DeckLoader()
{
    lua_close(pLuaState_);
}

bool DeckLoader::Configure()
{
    if (luaL_dofile(pLuaState_, "scripts/config.lua") != LUA_OK)
    {
        size_t len = 0;
        const char* pError = lua_tolstring(pLuaState_, -1, &len);
        error_.insert(0, pError, len);
        return false;
    }
    return true;
}

bool DeckLoader::Load()
{
    return false;
}

// lua interface setup

//! Unique IDs for all of the objects in the Lua interface to be used for type-checking
typedef enum : lua_Integer
{
    ID_MAP_KIT,
    ID_MAP_FACTORY,
    ID_RGB,
    ID_DECK_SETTINGS,
    ID_FACTORY_MAP
} LuaObjectID;

typedef enum : lua_Integer
{
    IDX_ICE_RUNNER = 1,
    IDX_OFFSET
} SpecialIndex;

//! Extract an object from a table on top of the stack.
//!
//! \tparam T_ The type to extract.
//! \param idx The absolute index of the table from which to extract the object.
//! \param targetID The ID of the object you want to extract.
//! \returns A pointer to the object on success, nullptr on failure.
//!
template <typename T_>
static inline T_* ExtractObject(lua_State* L, unsigned idx, LuaObjectID targetID)
{
    // [idx]: possible table
    if(!lua_istable(L, (int)idx)) return nullptr;
    lua_getfield(L, (int)idx, "_id");
    // [idx]: table
    // [-1]: possible id
    int isNum = 0;
    lua_Integer id = lua_tointegerx(L, -1, &isNum);
    if (!isNum || id != targetID)
    {
        lua_pop(L, 1);
        printf("Failed to extract object: invalid id: got %d, expected: %d\n", (int)id, (int)targetID);
        return nullptr;
    }

    // [idx]: table
    // [-1]: id
    lua_getfield(L, idx, "_instance");
    // [idx]: table
    // [-2]: id
    // [-1]: possible userdata
    T_* pUserData = (T_*)lua_touserdata(L, -1);
    if (!pUserData) printf("Failed to extract object: invalid userdata: %p\n", pUserData);
    // if the userdata is invalid or it is NULL, we will end up returning NULL, which is what we want.
    lua_pop(L, 2); // leave only the table that was originally on top of the stack.
    return pUserData;
}

template <typename T_>
static inline int ObjectGC(lua_State* L)
{
    T_* userdata = (T_*)lua_touserdata(L, -1);
    assert(userdata && "object not garbage collected correctly");
    userdata->~T_();
    return 0;
}

//! Create a new object an push it on the top of the stack.
//!
//! \tparam T_ The type of object to push.
//! \param reg The array of luaL_Reg to register to the new table.
//! \param id The id of object to create.
//! \note The c++ object must be default constructible for now.
//!
template <typename T_>
static inline T_* PushObject(lua_State* L,luaL_Reg reg[], LuaObjectID id)
{
    lua_newtable(L); // Lua instance table
    lua_newuserdata(L, sizeof(T_));
    // [-2]: instance table
    // [-1]: uninitialized instance userdata
    T_* pNewObj = (T_*)lua_touserdata(L, -1);
    new (pNewObj) T_; // construct the object in place.
    // [-2]: instance table
    // [-1]: initialized instance userdata
    lua_newtable(L); // create the userdata's metatable
    lua_pushcfunction(L, &ObjectGC<T_>); // push the __gc metamethod
    // [-4]: instance table
    // [-3]: initialized instance userdata
    // [-2]: empty metatable
    // [-1]: __gc metamethod
    lua_setfield(L, -2, "__gc");
    // [-3]: instance table
    // [-2]: initialized instance userdata
    // [-1]: metatable with __gc metamethod
    lua_setmetatable(L, -2);
    // [-2]: instance table
    // [-1]: initialized instance userdata with metatable
    lua_setfield(L, -2, "_instance");
    // [-1]: instance table
    lua_pushinteger(L, id);
    // [-2]: instance table
    // [-1]: object id
    lua_setfield(L, -2, "_id");
    // [-1]: instance table
    luaL_setfuncs(L, reg, 0); // register all functions into the instance table
    // [-1]: instance table
    return pNewObj;
}

class GeneralInterface
{
public:
    class RGBInterface
    {
    public:
        static int CallOperator(lua_State* L)
        {
            // [1]: RGB table
            lua_remove(L, 1);
            // [1..3]: possible R,G,B Numbers
            lua_Number r = luaL_checknumber(L, 1);
            lua_Number g = luaL_checknumber(L, 2);
            lua_Number b = luaL_checknumber(L, 3);
            luaL_Reg colorFunctions[] = {{nullptr, nullptr}};
            glm::vec3* pColor = PushObject<glm::vec3>(L, colorFunctions, ID_RGB);
            pColor->r = (float)r;
            pColor->g = (float)g;
            pColor->b = (float)b;
            // [1..3]: R,G,B Numbers (done with them)
            // RGB instance table
            return 1; // return the RGB instance table.
        }

        static bool Export(lua_State* L)
        {
            // [1]: IceRunner
            // [2]: General
            lua_newtable(L); // RGB
            lua_newtable(L); // RGB metatable
            lua_pushcfunction(L, &CallOperator); // __call metamethod
            // [1]: IceRunner
            // [2]: General
            // [3]: RGB table
            // [4]: RGB metatable
            // [5]: __call metafunction
            lua_setfield(L, -2, "__call");
            // [1]: IceRunner
            // [2]: General
            // [3]: RGB table
            // [4]: RGB metatable
            lua_setmetatable(L, -2);
            // [1]: IceRunner
            // [2]: General
            // [3]: RGB table
            lua_setfield(L, -2, "RGB");
            // [1]: IceRunner
            // [2]: General
            return true;
        }
    };

public:
    static bool Export(lua_State* L)
    {
        // [1]: IceRunner
        lua_newtable(L); // General table.
        bool result = RGBInterface::Export(L);
        lua_setfield(L, IDX_ICE_RUNNER, "General");
        // [1]: IceRunner
        return result;
    }
};

class MapToolsInterface
{
public:
    class MapKitInterface
    {
    public:
        //! Takes a table containing the size and walls of the map and returns a new MapKit
        static int CallOperator(lua_State* L)
        {
            // [1]: MapKit table
            // [2]: possible argument table
            lua_remove(L, 1);
            // [1]: possible argument table
            if (lua_gettop(L) != 1) return luaL_error(L, "Expected 1 argument.");
            if (!lua_istable(L, -1)) return luaL_argerror(L, 1, "Expected argument table.");

            lua_getfield(L, -1, "size");
            int isNum = 0;
            lua_Integer size = lua_tointegerx(L, -1, &isNum);
            if (!isNum) return luaL_argerror(L, 1, "Expected field of size => Integer.");
            // [1]: argument table
            // [2]: map size
            lua_getfield(L, 1, "walls");
            lua_Integer numWalls = lua_tointegerx(L, -1, &isNum);
            if (!isNum) return luaL_argerror(L, 1, "Expected field of walls => Integer.");
            // [1]: argument table
            // [2]: map size
            // [3]: num walls
            luaL_Reg kitFunctions[] = {{nullptr, nullptr}};
            MapKit* pKit = PushObject<MapKit>(L, kitFunctions, ID_MAP_KIT);
            // [1]: argument table
            // [2]: map size
            // [3]: num walls
            // [4]: MapKit instance table
            MapKit::Description desc;
            desc.dimensions.numRows = (uint32_t)size;
            desc.dimensions.numColumns = (uint32_t)size;
            desc.wallCount = (uint32_t)numWalls;
            pKit->SetDescription(desc);
            return 1; // return the instance table.
        }

        static bool Export(lua_State* L)
        {
            // [1]: IceRunner
            // [2]: MapTools
            lua_newtable(L); // MapKit
            lua_newtable(L); // MapKit metatable
            lua_pushcfunction(L, &CallOperator);
            // [1]: IceRunner
            // [2]: MapTools
            // [3]: MapKit metatable
            // [4]: __call metamethod
            lua_setfield(L, -2, "__call");
            // [1]: IceRunner
            // [2]: MapTools
            // [3]: MapKit metatable
            lua_setmetatable(L, -2);
            // [1]: IceRunner
            // [2]: MapTools
            lua_setfield(L, -2, "MapKit");
            return true;
        }
    };

public:
    static bool Export(lua_State* L)
    {
        // [1]: IceRunner
        lua_newtable(L); // MapTools
        bool result = MapKitInterface::Export(L);
        // [1]: IceRunner
        // [2]: MapTools
        lua_setfield(L, IDX_ICE_RUNNER, "MapTools");
        // [1]: IceRunner
        return result;
    }
};

class DeckSettingsInterface
{
public:
    class MapFactoryInterface
    {
    public:
        //! Return a new instance of MapFactory
        static int CallOperator(lua_State* L)
        {
            // [1]: MapFactory table
            // [2]: possible input table
            lua_remove(L, 1);
            // [1]: possible input table
            if (lua_gettop(L) != 1) return luaL_error(L, "Expected 1 argument");
            if (!lua_istable(L, -1)) return luaL_argerror(L, 1, "Table expected");
            // [1]: input table
            lua_getfield(L, 1, "map_color");
            // [1]: input table
            // [2]: possible RGB table
            glm::vec3* pMapColor = ExtractObject<glm::vec3>(L, 2, ID_RGB);
            if (!pMapColor) return luaL_argerror(L, 1, "Expected field of map_color => RGB");
            lua_getfield(L, 1, "player_color");
            // [1]: input table
            // [2]: RGB table (done with it)
            // [3]: possible RGB table
            glm::vec3* pPlayerColor = ExtractObject<glm::vec3>(L, 3, ID_RGB);
            if (!pPlayerColor) return luaL_argerror(L, 1, "Expected field of player_color => RGB");
            lua_getfield(L, 1, "kit");
            // [1]: input table
            // [2]: RGB table (done with it)
            // [3]: RGB table (done with it)
            // [4]: possible MapKit table
            MapKit* pKit = ExtractObject<MapKit>(L, 4, ID_MAP_KIT);
            if (!pKit) return luaL_error(L, "Expected field of kit => MapKit");
            // [1]: input table
            // [2]: RGB table (done with it)
            // [3]: RGB table (done with it)
            // [4]: MapKit table (done with it)
            luaL_Reg factoryFunctions[] = { {nullptr, nullptr} };
            MapFactory* pNewFactory = PushObject<MapFactory>(L, factoryFunctions, ID_MAP_FACTORY);
            pNewFactory->SetMapColor(*pMapColor);
            pNewFactory->SetPlayerColor(*pPlayerColor);
            pNewFactory->SetMapKit(*pKit);
            // [1]: input table
            // [2]: RGB table (done with it)
            // [3]: RGB table (done with it)
            // [4]: MapKit table (done with it)
            // [5]: MapFactory instance table
            return 1;
        }

        static bool Export(lua_State* L)
        {
            // [1]: IceRunner
            // [2]: DeckSettings
            lua_newtable(L); // MapFactory table
            lua_newtable(L); // MapFactory metatable
            lua_pushcfunction(L, &CallOperator);
            // [1]: IceRunner
            // [2]: DeckSettings
            // [3]: MapFactory table
            // [4]: MapFactory metatable
            // [5]: __call metamethod
            lua_setfield(L, -2, "__call");
            lua_setmetatable(L, -2);
            // [1]: IceRunner
            // [2]: DeckSettings
            // [3]: MapFactory table
            lua_setfield(L, -2, "MapFactory");
            // [1]: IceRunner
            // [2]: DeckSettings
            return true;
        }
    };

public:
    //! Returns a new instance of DeckSettings
    static int CallOperator(lua_State* L)
    {
        luaL_Reg reg[] =
        {
            {"set_factories", SetFactories},
            {nullptr, nullptr}
        };

        PushObject<DeckSettings>(L, reg, ID_DECK_SETTINGS);
        return 1; // return the instance
    }

    static int SetFactories(lua_State* L)
    {
        unsigned numArgs = lua_gettop(L);
        if (numArgs != 2) return luaL_error(L, "set_factories expected 2 arguments, got %d.", numArgs);

        // [1]: DeckSettings instance
        // [2]: factory map
        DeckSettings* pDeckSettings = ExtractObject<DeckSettings>(L, 1, ID_DECK_SETTINGS);
        if (!pDeckSettings) return luaL_error(L, "invalid first parameter to set_factories.");
        assert(lua_gettop(L) == 2 && "Bug in Extract");

        DeckLoader& loader = DeckLoader::GetInstance();
        // [1]: DeckSettings instance
        // [2]: factory map
        for(lua_Integer level = 1; true; level++, lua_pop(L, 1))
        {
            lua_geti(L, 2, level);
            MapFactory* pFactory = ExtractObject<MapFactory>(L, 3, ID_MAP_FACTORY);
            if (!pFactory) break;
            Map::Difficulty difficulty((Map::Difficulty::Rep)level);
            pFactory->SetDifficulty(difficulty);
            loader.GetDeckSettings().SetFactoryAt(difficulty, *pFactory);
        }

        return 0;
    }

    static bool Export(lua_State* L)
    {
        // [1]: IceRunner
        lua_newtable(L); // DeckSettings
        // [1]: IceRunner
        // [2]: DeckSettings
        lua_newtable(L); // DeckSettings metatable
        lua_pushcfunction(L, &CallOperator); // push the call metafunction
        // [1]: IceRunner
        // [2]: DeckSettings
        // [3]: DeckSettings metatable
        // [4]: __call metafunction
        lua_setfield(L, -2, "__call"); // add it to the DeckSettings metatable
        lua_setmetatable(L, -2); // set the DeckSettings metatable
        // [1]: IceRunner
        // [2]: DeckSettings
        if (!MapFactoryInterface::Export(L)) return false;
        lua_setfield(L, IDX_ICE_RUNNER, "DeckSettings");
        // [1]: IceRunner
        return true;
    }
};

static int IceConfigure(lua_State* L)
{
    return 0;
}

bool DeckLoader::ExportConfigInterface(lua_State* L)
{
    lua_newtable(L); // IceRunner

    bool result = DeckSettingsInterface::Export(L);
    result = result && MapToolsInterface::Export(L);
    result = result && GeneralInterface::Export(L);

    lua_pushcfunction(L, &IceConfigure);
    lua_setfield(L, IDX_ICE_RUNNER, "configure");

    lua_setglobal(L, "IceRunner");
    assert(lua_gettop(L) == 0 && "Stack should be empty");
    return result;
}

} // namespace game

} // namespace ice