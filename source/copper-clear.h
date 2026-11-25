// Copper-Clear V1.0, 25.11.2026
// Board-level straight polygons and rectangles. Geometry in millimeters.
// Included by pcb-gcode.ulp after its standard output helpers.
// Geometry from V0.1a; setup values use OUTPUT_UNITS. Inspect before machining.
real CC_TOOL_MM;
real CC_DEPTH_MM;
real CC_OVERLAP;
real CC_FEED_XY;
real CC_FEED_Z;
real CC_RPM;
int cc_dry_run = 0;
real CC_MARGIN = 0.005; // radial numerical allowance in mm

int cc_n;
real cc_ax[], cc_ay[], cc_bx[], cc_by[];
real cc_radius;
int cc_down, cc_cuts;
real cc_lastx, cc_lasty;

void cc_add_edge(real ax, real ay, real bx, real by)
{
    if (ax == bx && ay == by) return;
    cc_ax[cc_n] = ax; cc_ay[cc_n] = ay;
    cc_bx[cc_n] = bx; cc_by[cc_n] = by;
    cc_n++;
}

real cc_dist2(real x, real y, real ax, real ay, real bx, real by)
{
    real dx = bx-ax, dy = by-ay;
    real den = dx*dx+dy*dy;
    real t = 0.0;
    if (den > 0.0) t = ((x-ax)*dx+(y-ay)*dy)/den;
    t = max(0.0,min(1.0,t));
    dx = x-ax-t*dx; dy = y-ay-t*dy;
    return dx*dx+dy*dy;
}

real cc_cross(real ax, real ay, real bx, real by, real x, real y)
{
    return (bx-ax)*(y-ay)-(by-ay)*(x-ax);
}

int cc_intersects(real ax, real ay, real bx, real by,
                  real cx, real cy, real dx, real dy)
{
    real p = cc_cross(ax,ay,bx,by,cx,cy);
    real q = cc_cross(ax,ay,bx,by,dx,dy);
    real r = cc_cross(cx,cy,dx,dy,ax,ay);
    real s = cc_cross(cx,cy,dx,dy,bx,by);
    if (max(ax,bx)<min(cx,dx) || max(cx,dx)<min(ax,bx) ||
        max(ay,by)<min(cy,dy) || max(cy,dy)<min(ay,by)) return 0;
    return (p*q<=0.0 && r*s<=0.0);
}

int cc_same(real ax, real ay, real bx, real by)
{
    return abs(ax-bx)<0.0000001 && abs(ay-by)<0.0000001;
}

// Reject branching/open/self-intersecting outlines before generating paths.
int cc_valid_outline()
{
    int i, j;
    if (cc_n<3) return 0;
    for (i=0; i<cc_n; i++) {
        int a_count=0, b_count=0;
        for (j=0; j<cc_n; j++) {
            if (i==j) continue;
            int aa=cc_same(cc_ax[i],cc_ay[i],cc_ax[j],cc_ay[j]);
            int ab=cc_same(cc_ax[i],cc_ay[i],cc_bx[j],cc_by[j]);
            int ba=cc_same(cc_bx[i],cc_by[i],cc_ax[j],cc_ay[j]);
            int bb=cc_same(cc_bx[i],cc_by[i],cc_bx[j],cc_by[j]);
            a_count+=aa+ab; b_count+=ba+bb;
            if (aa+ab+ba+bb>1) return 0;
            if (aa+ab+ba+bb==0 && cc_intersects(cc_ax[i],cc_ay[i],cc_bx[i],cc_by[i],
                cc_ax[j],cc_ay[j],cc_bx[j],cc_by[j])) return 0;
        }
        if (a_count!=1 || b_count!=1) return 0;
    }
    return 1;
}

// The tool center must be inside the polygon AND clear of every edge.
int cc_safe_point(real x, real y)
{
    int i;
    int inside=0;
    real limit=cc_radius*cc_radius-0.000000001;
    for (i=0; i<cc_n; i++) {
        real ax=cc_ax[i], ay=cc_ay[i], bx=cc_bx[i], by=cc_by[i];
        if (cc_dist2(x,y,ax,ay,bx,by)<limit) return 0;
        if ((ay>y)!=(by>y)) {
            if (x < ax+(y-ay)*(bx-ax)/(by-ay)) inside=1-inside;
        }
    }
    return inside;
}

// Exact segment clearance: a non-intersecting pair of segments has
// minimum distance at an endpoint of at least one of them.
int cc_safe_link(real ax, real ay, real bx, real by)
{
    int i;
    if (!cc_safe_point(ax,ay) || !cc_safe_point(bx,by)) return 0;
    real limit=cc_radius*cc_radius-0.000000001;
    for (i=0; i<cc_n; i++) {
        real cx=cc_ax[i], cy=cc_ay[i], dx=cc_bx[i], dy=cc_by[i];
        if (cc_intersects(ax,ay,bx,by,cx,cy,dx,dy)) return 0;
        if (cc_dist2(ax,ay,cx,cy,dx,dy)<limit || cc_dist2(bx,by,cx,cy,dx,dy)<limit ||
            cc_dist2(cx,cy,ax,ay,bx,by)<limit || cc_dist2(dx,dy,ax,ay,bx,by)<limit) return 0;
    }
    return 1;
}

// OUTPUT-BEGIN -- Uses the existing PCB-Gcode postprocessor.
void cc_emit(real ax, real ay, real bx, real by)
{
    if (!cc_safe_link(ax,ay,bx,by)) return;
    if (cc_dry_run) { cc_cuts++; return; }
    if (cc_down && cc_safe_link(cc_lastx,cc_lasty,ax,ay)) {
        fxyr(scale_x(mm2u(ax)),scale_y(mm2u(ay)),CC_FEED_XY);
    } else {
        rz(DEFAULT_Z_UP);
        // Explicit XY rapid avoids suppression by the existing EPSILON.
        real tx=scale_x(mm2u(ax)), ty=scale_y(mm2u(ay));
        out(frr(RAPID_MOVE_XY,tx,ty)+EOL);
        update_cur_xy(tx,ty);
        fzr(COPPER_CLEAR_DEPTH,CC_FEED_Z);
    }
    fxyr(scale_x(mm2u(bx)),scale_y(mm2u(by)),CC_FEED_XY);
    cc_lastx=bx; cc_lasty=by; cc_down=1; cc_cuts++;
}
// OUTPUT-END

// Clip a line segment to the exact inward offset of the polygon.
// Coordinates along the line: (ox,oy)+t*(ux,uy). Unit direction.
// Events are intersections with boundary lines, their +/- radius offsets,
// and radius circles at edge endpoints. Extra events are harmless.
void cc_scan(real ox, real oy, real ux, real uy, real lo, real hi, int reverse)
{
    int i;
    real ev[]; int n=0;
    ev[n++]=lo; ev[n++]=hi;
    for (i=0; i<cc_n; i++) {
        real ax=(cc_ax[i]-ox)*ux+(cc_ay[i]-oy)*uy;
        real ay=-(cc_ax[i]-ox)*uy+(cc_ay[i]-oy)*ux;
        real bx=(cc_bx[i]-ox)*ux+(cc_by[i]-oy)*uy;
        real by=-(cc_bx[i]-ox)*uy+(cc_by[i]-oy)*ux;
        real dx=bx-ax, dy=by-ay;
        if (abs(dy)>0.000000001) {
            real length=sqrt(dx*dx+dy*dy);
            real base=ax-ay*dx/dy;
            real off=cc_radius*length/dy;
            ev[n++]=base; ev[n++]=base-off; ev[n++]=base+off;
        }
        if (abs(ay)<=cc_radius) {
            real h=sqrt(max(0.0,cc_radius*cc_radius-ay*ay));
            ev[n++]=ax-h; ev[n++]=ax+h;
        }
        if (abs(by)<=cc_radius) {
            real h=sqrt(max(0.0,cc_radius*cc_radius-by*by));
            ev[n++]=bx-h; ev[n++]=bx+h;
        }
    }
    // Clamp and insertion sort (portable ULP, no index-array dependency).
    for (i=0; i<n; i++) ev[i]=max(lo,min(hi,ev[i]));
    for (i=1; i<n; i++) {
        real v=ev[i]; int j=i;
        while (j>0 && ev[j-1]>v) { ev[j]=ev[j-1]; j--; }
        ev[j]=v;
    }
    real starts[], ends[]; int ns=0; int open_span=0;
    for (i=0; i<n-1; i++) {
        real a=ev[i], b=ev[i+1];
        if (b-a<0.000000001) continue;
        real m=(a+b)/2.0;
        if (cc_safe_point(ox+m*ux,oy+m*uy)) {
            if (open_span) ends[ns-1]=b;
            else { starts[ns]=a; ends[ns]=b; ns++; open_span=1; }
        } else open_span=0;
    }
    for (i=0; i<ns; i++) {
        int j=reverse ? ns-1-i : i;
        real a=reverse ? ends[j] : starts[j];
        real b=reverse ? starts[j] : ends[j];
        // Nudge endpoints into the interval to avoid roundoff at tangencies.
        real inset=min(0.0001,abs(b-a)/4.0);
        if (a<b) { a+=inset; b-=inset; }
        else { a-=inset; b+=inset; }
        cc_emit(ox+a*ux,oy+a*uy,ox+b*ux,oy+b*uy);
    }
}

// Emit a finishing segment, clipping if another edge restricts the offset.
void cc_finish_piece(real ax, real ay, real bx, real by)
{
    real dx=bx-ax, dy=by-ay;
    real len=sqrt(dx*dx+dy*dy);
    if (len<0.000000001) return;
    if (cc_safe_link(ax,ay,bx,by)) cc_emit(ax,ay,bx,by);
    else cc_scan(ax,ay,dx/len,dy/len,0.0,len,0);
}

// Reflex vertex: connect the inward parallel offsets by a circular join.
// Use circumscribed straight segments (G1), never chords inside the
// forbidden radius. Subdivide by normal-vector bisection, without atan2.
// A small outward deviation keeps every segment on the safe side.
void cc_reflex_join(real vx, real vy, real nx, real ny, real mx, real my)
{
    real xs[], ys[];
    int top=0;
    real r=cc_radius+0.0001;
    xs[0]=mx; ys[0]=my;
    while (top>=0) {
        real ex=xs[top], ey=ys[top];
        real dot=nx*ex+ny*ey;
        if (dot<0.9961946980917455) { // max 5 degrees per interval
            real dx=nx+ex, dy=ny+ey;
            real len=sqrt(dx*dx+dy*dy);
            if (len<0.000000001 || top>=64) return;
            top++;
            xs[top]=dx/len; ys[top]=dy/len;
        } else {
            // Intersection of the two tangents at radius r.
            real tx=vx+r*(nx+ex)/(1.0+dot);
            real ty=vy+r*(ny+ey)/(1.0+dot);
            cc_finish_piece(vx+r*nx,vy+r*ny,tx,ty);
            cc_finish_piece(tx,ty,vx+r*ex,vy+r*ey);
            nx=ex; ny=ey;
            top--;
        }
    }
}

void cc_paths(int which_side)
{
    int i, side, j, edge, pos, reverse_edge;
    int edge_order[], edge_dir[], edge_used[];
    real area2=0.0;
    real endx, endy, startx, starty;
    // Reconstruct a directed, closed outline even if EAGLE returns edges
    // out of order or individually reversed. No paths for an open outline.
    for (i=0; i<cc_n; i++) edge_used[i]=0;
    edge_order[0]=0; edge_dir[0]=1; edge_used[0]=1;
    endx=cc_bx[0]; endy=cc_by[0];
    for (i=1; i<cc_n; i++) {
        edge=-1;
        for (j=0; j<cc_n; j++) {
            if (edge_used[j]) continue;
            if (cc_same(endx,endy,cc_ax[j],cc_ay[j])) {
                edge=j; edge_dir[i]=1; break;
            }
            if (cc_same(endx,endy,cc_bx[j],cc_by[j])) {
                edge=j; edge_dir[i]=-1; break;
            }
        }
        if (edge<0) return;
        edge_order[i]=edge; edge_used[edge]=1;
        if (edge_dir[i]>0) { endx=cc_bx[edge]; endy=cc_by[edge]; }
        else { endx=cc_ax[edge]; endy=cc_ay[edge]; }
    }
    if (!cc_same(endx,endy,cc_ax[0],cc_ay[0])) return;
    // Signed twice-area; use an origin on the outline for numerical stability.
    startx=cc_ax[0]; starty=cc_ay[0];
    for (i=0; i<cc_n; i++) {
        edge=edge_order[i];
        area2+=edge_dir[i]*((cc_ax[edge]-startx)*(cc_by[edge]-starty)
                           -(cc_ay[edge]-starty)*(cc_bx[edge]-startx));
    }
    if (abs(area2)<0.000000001) return;
    // scale_x/scale_y reflect exactly one axis on Bottom unless MIRROR_BOTTOM
    // is enabled. FLIP_BOARD_IN_Y selects the axis, not the handedness.
    int reflected=(which_side==BOTTOM && MIRROR_BOTTOM==NO);
    int reverse_order=((area2>0.0)!=reflected);
    real xmin=cc_ax[0], xmax=xmin, ymin=cc_ay[0], ymax=ymin;
    for (i=0; i<cc_n; i++) {
        xmin=min(xmin,min(cc_ax[i],cc_bx[i])); xmax=max(xmax,max(cc_ax[i],cc_bx[i]));
        ymin=min(ymin,min(cc_ay[i],cc_by[i])); ymax=max(ymax,max(cc_ay[i],cc_by[i]));
    }
    real y0=ymin+cc_radius, y1=ymax-cc_radius;
    real step=CC_TOOL_MM*(1.0-CC_OVERLAP/100.0);
    if (y1<y0 || xmax-xmin<2.0*cc_radius) return;
    if (step<=0.0 || (y1-y0)/step>=10000.0) return;
    int rows=(y1-y0)/step;
    rows++;
    cc_down=0;
    for (i=0; i<=rows; i++) {
        real y=y0+(y1-y0)*i/rows;
        cc_scan(0.0,y,1.0,0.0,xmin,xmax,i%2);
    }
    // Cleanup clockwise in machine XY; complete reflex joins with G1 arcs.
    int inward=(area2>0.0) ? 1 : -1;
    if (reverse_order) inward=-inward;
    for (i=0; i<cc_n; i++) {
        pos=reverse_order ? cc_n-1-i : i;
        edge=edge_order[pos];
        reverse_edge=((edge_dir[pos]<0)!=reverse_order);
        real dx=cc_bx[edge]-cc_ax[edge], dy=cc_by[edge]-cc_ay[edge];
        real len=sqrt(dx*dx+dy*dy);
        real ux=dx/len, uy=dy/len;
        real r=cc_radius+0.0001;
        for (side=-1; side<=1; side+=2) {
            cc_scan(cc_ax[edge]-side*r*uy,cc_ay[edge]+side*r*ux,ux,uy,0.0,len,reverse_edge);
        }
        // The next edge in the directed traversal, including the closing join.
        int nextpos=reverse_order ? (pos+cc_n-1)%cc_n : (pos+1)%cc_n;
        int nextedge=edge_order[nextpos];
        int nextreverse=((edge_dir[nextpos]<0)!=reverse_order);
        real vx=reverse_edge ? cc_ax[edge] : cc_bx[edge];
        real vy=reverse_edge ? cc_ay[edge] : cc_by[edge];
        real dux=reverse_edge ? -ux : ux;
        real duy=reverse_edge ? -uy : uy;
        real ndx=cc_bx[nextedge]-cc_ax[nextedge];
        real ndy=cc_by[nextedge]-cc_ay[nextedge];
        real nlen=sqrt(ndx*ndx+ndy*ndy);
        real nux=nextreverse ? -ndx/nlen : ndx/nlen;
        real nuy=nextreverse ? -ndy/nlen : ndy/nlen;
        if (inward*(dux*nuy-duy*nux)<-0.000000001) {
            cc_reflex_join(vx,vy,-inward*duy,inward*dux,-inward*nuy,inward*nux);
        }
    }
    if (cc_down && !cc_dry_run) rz(DEFAULT_Z_UP);
    cc_down=0;
}

// Board data and file output, deliberately separate from geometry.
void cc_polygon(UL_POLYGON P)
{
    cc_n=0;
    P.wires(W) {
        if (W.curve!=0.0) { cc_n=0; return; }
        cc_add_edge(u2mm(W.x1),u2mm(W.y1),u2mm(W.x2),u2mm(W.y2));
    }
}

void cc_rectangle(UL_RECTANGLE R)
{
    int i;
    cc_n=0;
    real cx=(u2mm(R.x1)+u2mm(R.x2))/2.0;
    real cy=(u2mm(R.y1)+u2mm(R.y2))/2.0;
    real dx=abs(u2mm(R.x2)-u2mm(R.x1))/2.0;
    real dy=abs(u2mm(R.y2)-u2mm(R.y1))/2.0;
    real a=R.angle*3.141592653589793/180.0;
    real xx[], yy[];
    for (i=0; i<4; i++) {
        real x=(i==0 || i==3) ? -dx : dx;
        real y=(i<2) ? -dy : dy;
        xx[i]=cx+x*cos(a)-y*sin(a);
        yy[i]=cy+x*sin(a)+y*cos(a);
    }
    for (i=0; i<4; i++) cc_add_edge(xx[i],yy[i],xx[(i+1)%4],yy[(i+1)%4]);
}

int cc_configure()
{
    // The supplied generic postprocessor supports G20/G21, not mil/micron.
    if (OUTPUT_UNITS!=U_MILLIMETERS && OUTPUT_UNITS!=U_INCHES) return 0;
    CC_TOOL_MM=conv_units_from_to(COPPER_CLEAR_TOOL_SIZE,OUTPUT_UNITS,U_MILLIMETERS);
    CC_DEPTH_MM=conv_units_from_to(COPPER_CLEAR_DEPTH,OUTPUT_UNITS,U_MILLIMETERS);
    CC_OVERLAP=COPPER_CLEAR_OVERLAP;
    CC_FEED_XY=FEED_RATE_COPPER_CLEAR_XY;
    CC_FEED_Z=FEED_RATE_COPPER_CLEAR_Z;
    CC_RPM=SPINDLE_COPPER_CLEAR_RPM;
    if (CC_TOOL_MM<=0 || CC_DEPTH_MM>=0 || CC_OVERLAP<0 || CC_OVERLAP>=95 ||
        CC_FEED_XY<=0 || CC_FEED_Z<=0 || CC_RPM<=0 || DEFAULT_Z_UP<=0 ||
        DEFAULT_Z_HIGH<DEFAULT_Z_UP || EPSILON<0 || EPSILON>=DEFAULT_Z_UP-COPPER_CLEAR_DEPTH) return 0;
    cc_radius=CC_TOOL_MM/2.0+CC_MARGIN;
    return 1;
}

int cc_write_side(int which_side, int test_mode)
{
    int layer=-1, count=0, invalid=0;
    string lname=which_side==TOP ? "tCopperClear" : "bCopperClear";
    board(B) {
        B.layers(L) if (L.name==lname) layer=L.number;
        // A missing layer simply has no matching board objects.
        B.polygons(P) if (P.layer==layer) {
            cc_polygon(P);
            if (!cc_valid_outline()) invalid++;
            count++;
        }
        B.rectangles(R) if (R.layer==layer) {
            cc_rectangle(R);
            if (!cc_valid_outline()) invalid++;
            count++;
        }
    }
    string sideword=which_side==TOP ? TOP_FILE_NAME : BOT_FILE_NAME;
    string kind=test_mode ? "copperclear-test" : "copperclear";
    string path=m_file_name+"."+sideword+"."+kind+"."+DEFAULT_EXTENSION;
    if (count==0) {
        if (filetime(path)) dlgMessageBox("Copper-Clear: no area on "+lname+". Existing file is OLD, do not use: "+path);
        return 0;
    }
    if (invalid || !cc_configure()) {
        dlgMessageBox("Copper-Clear: invalid/curved outline or invalid tool, unit, feed or Z settings on "+lname+". No new file. Do not use an old file: "+path);
        return -1;
    }
    // Preflight every area before opening an NC file. An undersized area
    // must not leave a newly generated, incomplete output file.
    cc_dry_run=1;
    cc_cuts=0;
    board(B) {
        B.polygons(P) if (P.layer==layer) {
            cc_polygon(P);
            int before=cc_cuts;
            cc_paths(which_side);
            if (cc_cuts==before) invalid++;
        }
        B.rectangles(R) if (R.layer==layer) {
            cc_rectangle(R);
            int before=cc_cuts;
            cc_paths(which_side);
            if (cc_cuts==before) invalid++;
        }
    }
    cc_dry_run=0;
    if (invalid) {
        dlgMessageBox("Copper-Clear: at least one area has no toolpath on "+lname+". No new file. Do not use an old file: "+path);
        return -1;
    }
    g_side=which_side;
    cc_cuts=0;
    fileerror();
    output(path,"wt") {
        comm("Copper-Clear V1.0 - verify toolpaths before machining");
        comm("Layer: "+lname);
        comm(fr("Tool diameter mm: %.4f",CC_TOOL_MM));
        comm(fr("Cut depth mm: %.4f",CC_DEPTH_MM));
        comm(fr("Overlap percent: %.1f",CC_OVERLAP));
        begin_gcode(CC_RPM);
        board(B) {
            B.polygons(P) if (P.layer==layer) {
                cc_polygon(P);
                int before=cc_cuts;
                cc_paths(which_side);
                if (cc_cuts==before) invalid++;
            }
            B.rectangles(R) if (R.layer==layer) {
                cc_rectangle(R);
                int before=cc_cuts;
                cc_paths(which_side);
                if (cc_cuts==before) invalid++;
            }
        }
        end_gcode();
    }
    if (fileerror()) { dlgMessageBox("Copper-Clear: error writing "+path+". Do not use this file."); return -1; }
    if (invalid) {
        dlgMessageBox("Copper-Clear: some areas on "+lname+" have no toolpath. file is incomplete, do not use: "+path);
        return -1;
    }
    return cc_cuts;
}

void copper_clear_test()
{
    int top=cc_write_side(TOP, 1);
    int bot=cc_write_side(BOTTOM, 1);
    string msg;
    sprintf(msg,"Copper-Clear V1.0 TEST\nTop: %d cut segments\nBottom: %d cut segments\n0 = no area, -1 = error\nInspect the *.copperclear-test files in OCP. Do not machine until verified.",top,bot);
    dlgMessageBox(msg);
}
