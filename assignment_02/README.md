# Computer Architecture
TA email: chaerim.lim@yonsei.ac.kr

## Assignment 2: MIPS Assembly Programming
Deadline: 2025.05.02 23:59 (KST)

### MIPS Environment Setting
- For Window, You need to use VirtualBox/VMware/WSL
- Follow the below commands to setup
    ```
    # install spim simulator
    brew install spim        -- mac
    sudo apt install spim    -- others

    # check the spim version
    which spim -- the output might be looks like: /usr/bin/spim
    ```

- after setup, check spim simulator can be running correctly by typing spim
    ```
    spim
    ```
- type `quit` and press anter key to exit.

### Downloading Assignment
- Download the assignment zip file from LearnUs.
```
unzip ca2025_assignment2.zip
```

### Example MIPS Assembly code
1. Hello World!
```
spim -file ./examples/helloWorld.asm
```

2. add 1 to 10
```
spim -file ./examples/add1to10.asm
````

### Problems
- There are 3 problems you have to solve and submitt.
- Read the problem description in slides carefully.

#### P1: Reverse String (20 points)

#### P2: Fibonacci (30 points)

#### P3: Postfix Expression Calculator  (50 points: 30 + 20)

### Submission
- You need to **submit only a zip file** containing your asm files.
- The zip file should be named as `studentID.zip` (e.g., 2025123456.zip) and contain the following 3 files: `p1.asm, p2.asm, and p3.asm`.
- Your submittion zip file should look like this:
    ```
    studentID.zip
    ├── p1.asm
    ├── p2.asm
    └── p3.asm
    ```    
- You can use the following command to create a zip file:
    ```
    zip studentID.zip p1.asm p2.asm p3.asm
    ```
- You need to add some comments in your code to explain your logic.

> **Important!!**: Check your zip file that it contains the correct files and that they are named correctly. If you submit the wrong file or the wrong name, you will lose points.