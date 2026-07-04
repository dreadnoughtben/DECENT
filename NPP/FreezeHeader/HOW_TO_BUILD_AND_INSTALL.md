# How a .cpp becomes a Notepad++ plugin

## The Big Picture

Notepad++ does **not** run .cpp files directly.

1. You write C/C++ code (.cpp + .h)
2. You compile it with Visual Studio → this produces a .dll file
3. You put that .dll in a very specific folder
4. You restart Notepad++ → it loads the .dll and shows your commands in the Plugins menu

## Step-by-step Process (Current Project)

This folder contains a ready-to-adapt plugin based on the official template.

### 1. Open the project

- Open the solution file:
  `C:\Users\benja\npp-freeze-plugin\template\vs.proj\NppPluginTemplate.sln`

- Or open the .vcxproj directly.

### 2. Replace the main code

The important file is:
`C:\Users\benja\npp-freeze-plugin\src\NppPluginDemo.cpp`

We have already replaced its content with the freeze + hide-lines logic.

(You can also copy this file over the one in the template\src folder.)

### 3. Build the DLL

- Set platform to **x64** (very important)
- Set configuration to **Release** (for normal use) or **Debug**
- Build → Build Solution
- The resulting DLL will be somewhere like:
  `...\x64\Release\NppPluginDemo.dll`   (or similar name)

**Rename** the DLL to something nice, e.g. `FreezeHeader.dll`

### 4. Install the plugin

Notepad++ looks for plugins in this structure:

```
plugins\
    YourPluginName\
        YourPluginName.dll
```

**Recommended for development** (no admin rights):

1. Press Win+R and type:
   `%APPDATA%\Notepad++\plugins`
2. Create a folder called `FreezeHeader`
3. Put your `FreezeHeader.dll` inside it:

   ```
   %APPDATA%\Notepad++\plugins\FreezeHeader\FreezeHeader.dll
   ```

### 5. Load it

- Completely close Notepad++ (check Task Manager)
- Reopen Notepad++
- Go to menu: **Plugins → Freeze Header**

You should see "Enable Freeze (top 5)" and "Disable Freeze".

## Important Rules

- The **folder name** must exactly match the **DLL name** (without .dll)
- Must be **64-bit** DLL for 64-bit Notepad++
- You usually have to restart Notepad++ after replacing the DLL
- For development it's easier to use the `%APPDATA%` location

## Quick Test Workflow

1. Edit the .cpp in the src folder
2. Build in Visual Studio (x64 Release)
3. Copy the new .dll into `%APPDATA%\Notepad++\plugins\FreezeHeader\`
4. Restart Notepad++
5. Test

That's the entire process from .cpp → working plugin inside Notepad++.
