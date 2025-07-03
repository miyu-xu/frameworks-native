package android.test.math;

@VintfStability
interface IMath {
    int multiply(int a, int b);
    oneway void setCallback(android.test.math.IMathCallBack mathCallBack);
}