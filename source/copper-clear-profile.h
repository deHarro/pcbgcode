// Copper-Clear V1.0 profile migration for PCB-Gcode generated .def/.mac files.
// Included after pcb-gcode.h, filecopy.h. No active settings needed.
// Read and validate both files before touching either active header.
string cc_prof_compact(string text)
{
    string result="";
    for (int k=0; k<strlen(text); k++) {
        string ch=strsub(text,k,1);
        if (ch!=" " && ch!="\t" && ch!="\r" && ch!="\n") result+=ch;
    }
    return result;
}

// Only recognize active single-line declarations, not // commented examples.
// This is the format written by save_pcb_defaults/save_pcb_machine.
string cc_prof_value(string text, string key)
{
    string lines[];
    int n=strsplit(lines,text,'\n');
    for (int k=0; k<n; k++) {
        int eq=strstr(lines[k],"=");
        if (eq<0) continue;
        string lhs=cc_prof_compact(strsub(lines[k],0,eq));
        if (lhs!="int"+key && lhs!="real"+key && lhs!="string"+key) continue;
        string rhs=strsub(lines[k],eq+1);
        int semi=strstr(rhs,";");
        if (semi<0) continue;
        return cc_prof_compact(strsub(rhs,0,semi));
    }
    return "";
}

int cc_prof_units(string text)
{
    string u=cc_prof_value(text,"OUTPUT_UNITS");
    if (u=="U_MICRONS" || u=="0") return U_MICRONS;
    if (u=="U_MILLIMETERS" || u=="1") return U_MILLIMETERS;
    if (u=="U_MILS" || u=="2") return U_MILS;
    if (u=="U_INCHES" || u=="3") return U_INCHES;
    return -1;
}

string cc_prof_add(string text, string decl_type, string key, string value)
{
    if (cc_prof_value(text,key)!="") return text;
    return text+"\n"+decl_type+" "+key+" = "+value+"; // Copper-Clear profile migration\n";
}

string cc_prof_add_real(string text, string key, real value)
{
    string val;
    sprintf(val,"%.6f",value);
    return cc_prof_add(text,"real",key,val);
}

real cc_prof_from_mm(real v, int units)
{
    if (units==U_INCHES) return v/25.4;
    if (units==U_MILS) return v/0.0254;
    if (units==U_MICRONS) return v*1000.0;
    return v;
}

string cc_prof_upgrade_def(string text, int units)
{
    // Older PAD_CLEAR profiles also need these four definitions.
    text=cc_prof_add(text,"int","GENERATE_TOP_PAD_CLEAR","NO");
    text=cc_prof_add(text,"int","GENERATE_BOTTOM_PAD_CLEAR","NO");
    text=cc_prof_add_real(text,"PAD_CLEAR_DEPTH",cc_prof_from_mm(-0.03,units));
    text=cc_prof_add_real(text,"PAD_CLEAR_TOOL_SIZE",cc_prof_from_mm(0.2,units));
    // New operations are always disabled for legacy profiles.
    text=cc_prof_add(text,"int","GENERATE_TOP_COPPER_CLEAR","NO");
    text=cc_prof_add(text,"int","GENERATE_BOTTOM_COPPER_CLEAR","NO");
    text=cc_prof_add_real(text,"COPPER_CLEAR_DEPTH",cc_prof_from_mm(-0.069,units));
    text=cc_prof_add_real(text,"COPPER_CLEAR_TOOL_SIZE",cc_prof_from_mm(1.4,units));
    text=cc_prof_add_real(text,"COPPER_CLEAR_OVERLAP",50.0);
    return text;
}

string cc_prof_upgrade_mac(string text, int units)
{
    // Zero restores the legacy behavior without milling radius compensation.
    text=cc_prof_add_real(text,"MILLING_TOOL_SIZE",0.0);
    text=cc_prof_add_real(text,"FEED_RATE_PAD_CLEAR_XY",cc_prof_from_mm(300.0,units));
    text=cc_prof_add_real(text,"FEED_RATE_PAD_CLEAR_Z",cc_prof_from_mm(100.0,units));
    text=cc_prof_add_real(text,"SPINDLE_PAD_CLEAR_RPM",10000.0);
    text=cc_prof_add_real(text,"FEED_RATE_COPPER_CLEAR_XY",cc_prof_from_mm(300.0,units));
    text=cc_prof_add_real(text,"FEED_RATE_COPPER_CLEAR_Z",cc_prof_from_mm(100.0,units));
    text=cc_prof_add_real(text,"SPINDLE_COPPER_CLEAR_RPM",15000.0);
    return text;
}

string cc_prof_missing(string text, string keys)
{
    string names[];
    int n=strsplit(names,keys,' ');
    for (int k=0; k<n; k++) {
        if (cc_prof_value(text,names[k])=="") return names[k];
    }
    return "";
}

// Returns YES only after both headers were written successfully.
// Stored profiles are not modified by Load; Save can persist the upgrade.
int cc_prof_install(string def_path, string mac_path, string backup_suffix, int loading)
{
    string def_text, mac_text;
    string active_def=g_path+"/settings/pcb-defaults.h";
    string active_mac=g_path+"/settings/pcb-machine.h";
    fileerror();
    fileread(def_text,def_path);
    if (fileerror() || def_text=="") {
        dlgMessageBox("Cannot read profile: "+def_path); return NO;
    }
    fileerror();
    fileread(mac_text,mac_path);
    if (fileerror() || mac_text=="") {
        dlgMessageBox("Cannot read profile: "+mac_path); return NO;
    }
    int units=cc_prof_units(def_text);
    if (units<0) { dlgMessageBox("Missing or unknown OUTPUT_UNITS: "+def_path); return NO; }
    // Reject files missing core PCB-Gcode declarations rather than guessing.
    string missing=cc_prof_missing(def_text,
        "SINGLE_PASS ISO_MIN ISO_MAX ISO_STEP GENERATE_TOP_OUTLINES GENERATE_TOP_DRILL GENERATE_TOP_FILL GENERATE_TOP_STENCIL GENERATE_BOTTOM_OUTLINES GENERATE_BOTTOM_DRILL GENERATE_BOTTOM_FILL GENERATE_BOTTOM_STENCIL MIRROR_BOTTOM SIMPLE_DRILL_CODE GENERATE_MILLING CLIMB_MILLING GENERATE_TEXT SPOT_DRILL SPOT_DRILL_DEPTH DO_TOOL_CHANGE_WITH_ZERO_STEP FLIP_BOARD_IN_Y OUTPUT_UNITS HOLE_MILL_STYLE NC_OPERATOR_MESSAGE PREVIEW_WINDOW_WIDTH PREVIEW_WINDOW_HEIGHT");
    if (missing=="") missing=cc_prof_missing(mac_text,
        "DEFAULT_Z_HIGH DEFAULT_Z_UP DEFAULT_Z_DOWN DRILL_DEPTH DRILL_DWELL SPINDLE_ON_TIME SPINDLE_ETCH_RPM SPINDLE_DRILL_RPM SPINDLE_MILL_RPM SPINDLE_TEXT_RPM SPINDLE_STENCIL_RPM MILLING_DEPTH TEXT_DEPTH TOOL_CHANGE_POS_X TOOL_CHANGE_POS_Y TOOL_CHANGE_POS_Z FEED_RATE_ETCH_XY FEED_RATE_ETCH_Z FEED_RATE_DRILL_XY FEED_RATE_DRILL_Z FEED_RATE_MILL_XY FEED_RATE_MILL_Z FEED_RATE_TEXT_XY FEED_RATE_TEXT_Z FEED_RATE_STENCIL_XY FEED_RATE_STENCIL_Z STENCIL_TOOL_SIZE TOOL_SIZE X_OFFSET Y_OFFSET X_HOME Y_HOME EPSILON");
    if (missing!="") { dlgMessageBox("Profile not loaded. Missing core setting: "+missing); return NO; }
    string new_def=cc_prof_upgrade_def(def_text,units);
    string new_mac=cc_prof_upgrade_mac(mac_text,units);
    if (!loading && new_def==def_text && new_mac==mac_text) return YES;
    // Back up both active files before replacing either of them.
    filecopy(active_def,active_def+backup_suffix);
    filecopy(active_mac,active_mac+backup_suffix);
    fileerror();
    output(active_def,"wt") { printf("%s",new_def); }
    if (fileerror()) {
        dlgMessageBox("Could not write "+active_def+". Restore both backups: "+backup_suffix); return NO;
    }
    fileerror();
    output(active_mac,"wt") { printf("%s",new_mac); }
    if (fileerror()) {
        dlgMessageBox("Could not write "+active_mac+". Restore both backups: "+backup_suffix); return NO;
    }
    return YES;
}
