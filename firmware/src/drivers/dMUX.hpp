#ifndef dMUX_H
#define dMUX_H

class dMUX {
    
    public:
        dMUX();

        enum receiver_configuration {
            HF,
            VHF_CHARLES,
            VHF_EXTERNAL
        };

        // Public API
        int Get_Receiver_Configuration_State();
        void Set_Receiver_Configuration_State(receiver_configuration configuration_number);

    private:
        void Configure_For_VHF_Charles();
        void Configure_For_VHF_External();
        void Configure_For_HF();

        receiver_configuration state = receiver_configuration::HF;
        
        dMUX(const dMUX&);
        void operator=(const dMUX&);
};

extern dMUX g_MUX;

#endif  // dMUX_H