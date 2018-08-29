
string deriv2FS = str_fmt(crypt(R"glsl(
        const highp float dw = %f, dh = %f;
        const highp float mb = %f;

        float gray(float xoff, float yoff){
            vec3 p = texture(tex0, vec2(coord.x + xoff, coord.y + yoff)).rgb;
            return p.r * .2125 + p.g * .7154 + p.b * .0721;
        }
        void main() {
            highp float p = gray(0., 0.), pLeft = gray(-dw, 0.), pTop = gray(0., -dh), pRight = gray(dw, 0.), pDown = gray(0., dh);
            highp float xDer2 = (2. * p - pRight - pLeft) * mb, yDer2 = (2. * p - pDown - pTop) * mb, xyDer = ((pRight - pLeft) - (pDown - pTop)) * mb;
            color = vec4(xDer2 / 2. + .5, yDer2 / 2. + .5, xyDer / 2. + .5, 1);
        }
    )glsl"), dw, dh, mulBias);

string hessianFS = str_fmt(crypt(R"glsl(
        const highp float mb = %f;
        //const highp float mbmb = mb * mb;

        void main() {
            vec3 d = texture(tex0, coord).xyz;
            highp float sIxx = (d.x - .5) * 2. / mb, sIyy = (d.y - .5) * 2. / mb, sIxy = (d.z - .5) * 2. / mb;

            highp float h = (sIxx * sIyy - sIxy * sIxy) / (sIxx + sIyy) / (sIxx + sIyy);
            highp float h1 = h;
            highp float h2 = mod(h1 * 256., 1.);

            color = vec4(h1, h2, 0, 1);
        }
    )glsl"), mulBias);

