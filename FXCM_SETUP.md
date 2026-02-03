# 🏦 FXCM ForexConnect Integration Guide

> **CRITICAL: READ THIS BEFORE BUILDING CRE.mn**
> 
> Agents keep forgetting this setup. This document is the SINGLE SOURCE OF TRUTH.

---

## ⚡ Quick Check: Is FXCM Enabled?

```bash
# In WSL:
grep FXCM_ENABLED /mnt/c/workshop/repo/LOB/build/CMakeCache.txt
```

**Expected:** `FXCM_ENABLED:BOOL=ON`

If it shows `OFF`, you need to rebuild!

---

## 📁 SDK Locations (VERIFIED 2026-06-04)

### Linux SDK (for WSL builds):
```
C:\workshop\repo\LOB\deps\fxcm-linux\ForexConnectAPI-1.6.5-Linux-x86_64\
├── include/
│   ├── ForexConnect.h
│   └── forexconnect/*.h
└── lib/
    ├── libForexConnect.so
    ├── libForexConnect.so.1.6.5
    ├── libpricehistorymgr.so
    └── libquotesmgr2.so
```

### Windows SDK (for native Windows builds):
```
C:\Candleworks\ForexConnectAPIx64\
├── include/
│   ├── ForexConnect.h
│   └── forexconnect/*.h
└── lib/
    ├── ForexConnect.lib
    ├── pricehistorymgr.lib
    └── quotesmgr2.lib
```

---

## 🔧 CMakeLists.txt Path Configuration

The CMake looks for SDK at:
- **Linux:** `${CMAKE_CURRENT_SOURCE_DIR}/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64`
- **Windows:** `${CMAKE_CURRENT_SOURCE_DIR}/deps/fxcm`

### Key Check in CMake:
```cmake
if(EXISTS "${FXCM_SDK_DIR}/include/ForexConnect.h")
    # FXCM will be enabled
else()
    # FXCM will be DISABLED - CHECK SDK PATH!
endif()
```

---

## 🛠️ Build Commands (WSL)

### Full Rebuild with FXCM:
```bash
cd /mnt/c/workshop/repo/LOB
rm -rf build
mkdir build && cd build
cmake -DFXCM_ENABLED=ON ..
make -j$(nproc)

# Verify FXCM is enabled:
grep FXCM_ENABLED CMakeCache.txt
# MUST show: FXCM_ENABLED:BOOL=ON
```

### Start Server with FXCM:
```bash
cd /mnt/c/workshop/repo/LOB/build
export LD_LIBRARY_PATH=/mnt/c/workshop/repo/LOB/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib
export FXCM_USER=8000065022
export FXCM_PASS=Meniscus_321957
export FXCM_CONNECTION=Real
export ADMIN_API_KEY=cre2026admin
./central_exchange
```

---

## 🔍 Troubleshooting

### Problem: `FXCM_ENABLED:BOOL=OFF` despite `-DFXCM_ENABLED=ON`

**Cause:** SDK not found at expected path

**Fix:**
```bash
# Check if SDK exists:
ls -la /mnt/c/workshop/repo/LOB/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/include/ForexConnect.h

# If missing, the SDK was extracted to wrong location. Look for it:
find /mnt/c/workshop -name "libForexConnect.so" 2>/dev/null

# Move it to correct location:
mv /path/to/ForexConnectAPI-1.6.5-Linux-x86_64 /mnt/c/workshop/repo/LOB/deps/fxcm-linux/
```

### Problem: Server shows "fxcm: connected" but prices are simulated

**Cause:** Binary was built with `FXCM_ENABLED=OFF`

**Check:**
```bash
# If you see this warning at build time:
# "FXCM_ENABLED=ON but ForexConnect SDK not found"
# The binary will run simulation mode!
```

**Fix:** Rebuild after fixing SDK path.

### Problem: Library not found at runtime

**Error:** `error while loading shared libraries: libForexConnect.so: cannot open shared object file`

**Fix:** Set LD_LIBRARY_PATH before running:
```bash
export LD_LIBRARY_PATH=/mnt/c/workshop/repo/LOB/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib
```

---

## 📋 Startup Checklist

Before starting CRE.mn, verify:

- [ ] `grep FXCM_ENABLED build/CMakeCache.txt` shows `ON`
- [ ] `deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/include/ForexConnect.h` exists
- [ ] `deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib/libForexConnect.so` exists
- [ ] Environment variables set (FXCM_USER, FXCM_PASS, etc.)
- [ ] LD_LIBRARY_PATH includes SDK lib path

---

## 🔐 FXCM Credentials

```
Account: 8000065022
Password: Meniscus_321957
Connection: Real (not Demo!)
```

---

## 📜 History

| Date | Action | Agent |
|------|--------|-------|
| 2026-06-04 | Moved SDK to deps/fxcm-linux/, rebuilt with FXCM_ENABLED=ON | NEXUS |
| 2026-01-30 | CMake option documented, RPATH set to avoid LD_LIBRARY_PATH | - |
| 2026-01-29 | Initial FXCM integration | - |

---

**Remember:** If /health shows "fxcm: connected" but you're getting simulated prices, CHECK `FXCM_ENABLED` IN CMAKECACHE!
